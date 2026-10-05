#!/usr/bin/env python3
"""
Read and write compiled ("binary") Android XML, such as an APK's AndroidManifest.xml, with no third-party deps.

The file is decoded into a list of nodes (namespaces, elements, attributes, text), edited as plain Python
objects, and encoded again from scratch: a new string pool and resource map are built from what the nodes use,
so adding or removing strings never shifts an index by mistake.

    python3 axml.py dump AndroidManifest.xml        # print it as readable XML
    python3 axml.py roundtrip AndroidManifest.xml   # decode + encode + decode, and check nothing changed
"""
import struct
import sys

RES_XML_TYPE = 0x0003
RES_STRING_POOL_TYPE = 0x0001
RES_XML_RESOURCE_MAP_TYPE = 0x0180
RES_XML_START_NAMESPACE_TYPE = 0x0100
RES_XML_END_NAMESPACE_TYPE = 0x0101
RES_XML_START_ELEMENT_TYPE = 0x0102
RES_XML_END_ELEMENT_TYPE = 0x0103
RES_XML_CDATA_TYPE = 0x0104

TYPE_NULL = 0x00
TYPE_REFERENCE = 0x01
TYPE_STRING = 0x03
TYPE_INT_DEC = 0x10
TYPE_INT_HEX = 0x11
TYPE_INT_BOOLEAN = 0x12

ANDROID_NS = "http://schemas.android.com/apk/res/android"
NO_INDEX = 0xFFFFFFFF


class Attr:
    """One attribute. `value` is a str when type_ is TYPE_STRING, otherwise the raw 32-bit data."""

    def __init__(self, name, type_, value, ns=ANDROID_NS, resid=None, raw=None):
        self.ns, self.name, self.resid = ns, name, resid
        self.type, self.value = type_, value
        self.raw = raw if raw is not None else (value if type_ == TYPE_STRING else None)

    def key(self):
        return (self.ns, self.name, self.resid, self.type, self.value, self.raw)

    def __repr__(self):
        return "Attr(%s=%r)" % (self.name, self.value)


class Node:
    """kind: 'ns_start' / 'ns_end' (prefix, uri), 'start' (ns, name, attrs), 'end' (ns, name), 'cdata' (text)."""

    def __init__(self, kind, **kw):
        self.kind = kind
        self.line = kw.get("line", 0)
        self.comment = kw.get("comment")
        self.prefix = kw.get("prefix")
        self.uri = kw.get("uri")
        self.ns = kw.get("ns")
        self.name = kw.get("name")
        self.attrs = kw.get("attrs", [])
        self.text = kw.get("text")
        self.cdata_type = kw.get("cdata_type", (TYPE_NULL, 0))

    def attr(self, name, ns=ANDROID_NS):
        for a in self.attrs:
            if a.name == name and a.ns == ns:
                return a
        return None

    def key(self):
        return (self.kind, self.line, self.comment, self.prefix, self.uri, self.ns, self.name,
                tuple(a.key() for a in self.attrs), self.text)


class Doc:
    def __init__(self, nodes, utf8):
        self.nodes, self.utf8 = nodes, utf8

    # ---------------- editing helpers ----------------
    def elements(self, name):
        return [n for n in self.nodes if n.kind == "start" and n.name == name]

    def end_of(self, start):
        """Index of the matching end node for a start node."""
        i = self.nodes.index(start)
        depth = 0
        for j in range(i, len(self.nodes)):
            k = self.nodes[j].kind
            if k == "start":
                depth += 1
            elif k == "end":
                depth -= 1
                if depth == 0:
                    return j
        raise ValueError("unbalanced XML")

    def remove_element(self, start):
        i, j = self.nodes.index(start), self.end_of(start)
        del self.nodes[i:j + 1]

    def insert_before_end(self, parent, nodes):
        j = self.end_of(parent)
        self.nodes[j:j] = nodes

    # ---------------- text view ----------------
    def to_text(self):
        out, depth, pending_ns = [], 0, []
        prefixes = {}
        for n in self.nodes:
            if n.kind == "ns_start":
                prefixes[n.uri] = n.prefix
                pending_ns.append(n)
            elif n.kind == "start":
                parts = [n.name]
                for ns in pending_ns:
                    parts.append('xmlns:%s="%s"' % (ns.prefix, ns.uri))
                pending_ns = []
                for a in n.attrs:
                    nm = (prefixes.get(a.ns, "?") + ":" + a.name) if a.ns else a.name
                    parts.append('%s="%s"' % (nm, _fmt_value(a)))
                out.append("  " * depth + "<" + " ".join(parts) + ">")
                depth += 1
            elif n.kind == "end":
                depth -= 1
                out.append("  " * depth + "</" + n.name + ">")
            elif n.kind == "cdata":
                out.append("  " * depth + n.text)
        return "\n".join(out) + "\n"

    # ---------------- encoding ----------------
    def encode(self):
        # 1) attribute names that carry a resource id come first in the pool, in resource-map order
        attr_slots, attr_index = [], {}
        for n in self.nodes:
            if n.kind == "start":
                for a in n.attrs:
                    if a.resid is not None and (a.name, a.resid) not in attr_index:
                        attr_index[(a.name, a.resid)] = len(attr_slots)
                        attr_slots.append((a.name, a.resid))
        strings = [s for s, _ in attr_slots]
        general = {}

        def sidx(s):
            if s is None:
                return NO_INDEX
            if s not in general:
                general[s] = len(strings)
                strings.append(s)
            return general[s]

        body = bytearray()
        for n in self.nodes:
            if n.kind in ("ns_start", "ns_end"):
                t = RES_XML_START_NAMESPACE_TYPE if n.kind == "ns_start" else RES_XML_END_NAMESPACE_TYPE
                body += _node(t, n.line, sidx(n.comment), struct.pack("<II", sidx(n.prefix), sidx(n.uri)))
            elif n.kind == "start":
                attrs = b""
                id_i = class_i = style_i = 0
                for k, a in enumerate(n.attrs):
                    name_i = attr_index[(a.name, a.resid)] if a.resid is not None else sidx(a.name)
                    data = sidx(a.value) if a.type == TYPE_STRING else (a.value & 0xFFFFFFFF)
                    attrs += struct.pack("<III", sidx(a.ns), name_i, sidx(a.raw))
                    attrs += struct.pack("<HBBI", 8, 0, a.type, data)
                    if a.name == "id" and a.ns == ANDROID_NS:
                        id_i = k + 1
                    elif a.name == "class" and a.ns is None:
                        class_i = k + 1
                    elif a.name == "style" and a.ns is None:
                        style_i = k + 1
                ext = struct.pack("<IIHHHHHH", sidx(n.ns), sidx(n.name), 0x14, 0x14, len(n.attrs), id_i, class_i, style_i)
                body += _node(RES_XML_START_ELEMENT_TYPE, n.line, sidx(n.comment), ext + attrs)
            elif n.kind == "end":
                body += _node(RES_XML_END_ELEMENT_TYPE, n.line, sidx(n.comment), struct.pack("<II", sidx(n.ns), sidx(n.name)))
            elif n.kind == "cdata":
                ct, cd = n.cdata_type
                ext = struct.pack("<I", sidx(n.text)) + struct.pack("<HBBI", 8, 0, ct, cd)
                body += _node(RES_XML_CDATA_TYPE, n.line, sidx(n.comment), ext)

        pool = _encode_pool(strings, self.utf8)
        resmap = struct.pack("<HHI", RES_XML_RESOURCE_MAP_TYPE, 8, 8 + 4 * len(attr_slots))
        resmap += struct.pack("<%dI" % len(attr_slots), *[r for _, r in attr_slots])
        content = pool + resmap + bytes(body)
        return struct.pack("<HHI", RES_XML_TYPE, 8, 8 + len(content)) + content


def _fmt_value(a):
    if a.type == TYPE_STRING:
        return a.value.replace('"', "&quot;")
    if a.type == TYPE_INT_BOOLEAN:
        return "true" if a.value else "false"
    if a.type == TYPE_REFERENCE:
        return "@0x%08x" % a.value
    if a.type == TYPE_INT_HEX:
        return "0x%x" % a.value
    if a.type == TYPE_INT_DEC:
        return str(a.value - (1 << 32) if a.value & 0x80000000 else a.value)
    return "(type 0x%x) 0x%x" % (a.type, a.value)


def _node(type_, line, comment, ext):
    # every XML tree node: ResChunk_header(8) + lineNumber + comment = a 16-byte header, then the extension
    return struct.pack("<HHIII", type_, 16, 16 + len(ext), line, comment) + ext


def _len8(n):
    if n > 0x7FFF:
        raise ValueError("string too long for a UTF-8 pool")
    return bytes([(n >> 8) | 0x80, n & 0xFF]) if n > 0x7F else bytes([n])


def _encode_pool(strings, utf8):
    data, offsets = bytearray(), []
    for s in strings:
        offsets.append(len(data))
        if utf8:
            b = s.encode("utf-8")
            data += _len8(len(s.encode("utf-16-le")) // 2) + _len8(len(b)) + b + b"\x00"
        else:
            b = s.encode("utf-16-le")
            n = len(b) // 2
            data += (struct.pack("<H", n) if n <= 0x7FFF else struct.pack("<HH", (n >> 16) | 0x8000, n & 0xFFFF)) + b + b"\x00\x00"
    while len(data) % 4:
        data += b"\x00"
    count = len(strings)
    header = struct.pack("<HHI", RES_STRING_POOL_TYPE, 28, 28 + 4 * count + len(data))
    header += struct.pack("<IIIII", count, 0, 0x100 if utf8 else 0, 28 + 4 * count, 0)
    return header + struct.pack("<%dI" % count, *offsets) + bytes(data)


def _decode_pool(raw, off):
    count, style_count, flags, strings_start, _ = struct.unpack_from("<IIIII", raw, off + 8)
    if style_count:
        raise ValueError("styled strings in an XML pool are not supported")
    utf8 = bool(flags & 0x100)
    offsets = struct.unpack_from("<%dI" % count, raw, off + 28)
    base = off + strings_start
    out = []
    for o in offsets:
        p = base + o
        if utf8:
            for _ in range(2):   # UTF-16 length, then byte length
                v = raw[p]; p += 1
                if v & 0x80:
                    v = ((v & 0x7F) << 8) | raw[p]; p += 1
            out.append(raw[p:p + v].decode("utf-8", "replace"))
        else:
            v, = struct.unpack_from("<H", raw, p); p += 2
            if v & 0x8000:
                lo, = struct.unpack_from("<H", raw, p); p += 2
                v = ((v & 0x7FFF) << 16) | lo
            out.append(raw[p:p + 2 * v].decode("utf-16-le", "replace"))
    return out, utf8


def decode(raw):
    t, _, total = struct.unpack_from("<HHI", raw, 0)
    if t != RES_XML_TYPE:
        raise ValueError("not a binary XML file")
    strings, utf8, resmap, nodes = None, True, [], []
    off = 8

    def S(i):
        return None if i == NO_INDEX else strings[i]

    while off < total:
        ctype, hsize, csize = struct.unpack_from("<HHI", raw, off)
        if ctype == RES_STRING_POOL_TYPE and strings is None:
            strings, utf8 = _decode_pool(raw, off)
        elif ctype == RES_XML_RESOURCE_MAP_TYPE:
            resmap = list(struct.unpack_from("<%dI" % ((csize - hsize) // 4), raw, off + hsize))
        elif ctype in (RES_XML_START_NAMESPACE_TYPE, RES_XML_END_NAMESPACE_TYPE, RES_XML_START_ELEMENT_TYPE,
                       RES_XML_END_ELEMENT_TYPE, RES_XML_CDATA_TYPE):
            line, comment = struct.unpack_from("<II", raw, off + 8)
            e = off + hsize
            common = dict(line=line, comment=S(comment))
            if ctype in (RES_XML_START_NAMESPACE_TYPE, RES_XML_END_NAMESPACE_TYPE):
                p, u = struct.unpack_from("<II", raw, e)
                nodes.append(Node("ns_start" if ctype == RES_XML_START_NAMESPACE_TYPE else "ns_end", prefix=S(p), uri=S(u), **common))
            elif ctype == RES_XML_START_ELEMENT_TYPE:
                ns, name, astart, asize, acount = struct.unpack_from("<IIHHH", raw, e)
                attrs = []
                for k in range(acount):
                    a = e + astart + k * asize
                    ans, aname, araw = struct.unpack_from("<III", raw, a)
                    _, _, dtype, data = struct.unpack_from("<HBBI", raw, a + 12)
                    resid = resmap[aname] if aname < len(resmap) and resmap[aname] else None
                    value = S(data) if dtype == TYPE_STRING else data
                    attrs.append(Attr(S(aname), dtype, value, ns=S(ans), resid=resid, raw=S(araw)))
                nodes.append(Node("start", ns=S(ns), name=S(name), attrs=attrs, **common))
            elif ctype == RES_XML_END_ELEMENT_TYPE:
                ns, name = struct.unpack_from("<II", raw, e)
                nodes.append(Node("end", ns=S(ns), name=S(name), **common))
            else:
                d, = struct.unpack_from("<I", raw, e)
                _, _, ct, cd = struct.unpack_from("<HBBI", raw, e + 4)
                nodes.append(Node("cdata", text=S(d), cdata_type=(ct, cd), **common))
        off += csize
    return Doc(nodes, utf8)


def same(a, b):
    return [n.key() for n in a.nodes] == [n.key() for n in b.nodes]


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("dump", "roundtrip"):
        sys.exit(__doc__)
    raw = open(sys.argv[2], "rb").read()
    doc = decode(raw)
    if sys.argv[1] == "dump":
        sys.stdout.write(doc.to_text())
    else:
        again = decode(doc.encode())
        print("round trip OK" if same(doc, again) else "round trip CHANGED SOMETHING")
        sys.exit(0 if same(doc, again) else 1)
