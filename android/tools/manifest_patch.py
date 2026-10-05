#!/usr/bin/env python3
"""
Add two components to a compiled (binary) AndroidManifest.xml, without decompiling the whole APK:

  <activity  android:name="com.bowlingplus.Pickers$Relay"
             android:theme="@android:style/Theme.Translucent.NoTitleBar"
             android:excludeFromRecents="true" android:exported="false"/>
  <provider  android:name="com.bowlingplus.BpFileProvider"
             android:authorities="<pkg>.bowlingplus.fileprovider"
             android:exported="false" android:grantUriPermissions="true"/>

The Relay activity runs the system photo/file picker and relays its result to the tweak (Pickers.java); the
provider (our own tiny read-only ContentProvider, so no @xml resource is needed) lets the menu share QR codes
and the pin template. We use a distinct authority so there's no clash with the game's own providers.

Binary AXML is a chunk stream: a string pool, a resource-map, then START/END element and namespace chunks.
To add elements we (1) append any new strings to the pool, (2) append the needed resource-ids to the
resource map, and (3) splice new START/END element chunks in just before </application>. Attribute values
that are resource references need their ids in the resource map; everything else is a string or an inline
int. The one external resource we add (@xml/bp_file_paths) is written by the build as a new resource; its id
is passed in.

This file has no third-party deps so it can run in CI and be unit-tested on the real manifest.
"""
import struct

RES_XML_TYPE = 0x0003
RES_STRING_POOL_TYPE = 0x0001
RES_XML_RESOURCE_MAP_TYPE = 0x0180
RES_XML_START_NAMESPACE_TYPE = 0x0100
RES_XML_END_NAMESPACE_TYPE = 0x0101
RES_XML_START_ELEMENT_TYPE = 0x0102
RES_XML_END_ELEMENT_TYPE = 0x0103

TYPE_REFERENCE = 0x01
TYPE_STRING = 0x03
TYPE_INT_DEC = 0x10
TYPE_INT_BOOLEAN = 0x12

# framework attribute resource ids (stable public ids)
ATTR = {
    "name": 0x01010003,
    "theme": 0x01010000,
    "exported": 0x01010010,
    "authorities": 0x01010018,
    "grantUriPermissions": 0x0101001b,
    "excludeFromRecents": 0x01010017,
    "resource": 0x01010025,
}
THEME_TRANSLUCENT = 0x01030056   # @android:style/Theme.Translucent.NoTitleBar


class StringPool:
    def __init__(self, raw, off):
        self.off = off
        hdr_size, = struct.unpack_from("<H", raw, off + 2)
        self.size, = struct.unpack_from("<I", raw, off + 4)
        (self.count, self.style_count, self.flags, self.strings_start, self.styles_start) = struct.unpack_from("<IIIII", raw, off + 8)
        self.utf8 = bool(self.flags & 0x100)
        self.offsets = list(struct.unpack_from("<%dI" % self.count, raw, off + 28))
        self.data_start = off + self.strings_start
        self.raw = raw
        self.strings = [self._read(i) for i in range(self.count)]

    def _read(self, i):
        p = self.data_start + self.offsets[i]
        if self.utf8:
            n16, p = self._len8(p)
            n8, p = self._len8(p)
            return self.raw[p:p + n8].decode("utf-8", "replace")
        n, p = self._len16(p)
        return self.raw[p:p + n * 2].decode("utf-16-le", "replace")

    def _len8(self, p):
        v = self.raw[p]; p += 1
        if v & 0x80:
            v = ((v & 0x7f) << 8) | self.raw[p]; p += 1
        return v, p

    def _len16(self, p):
        v, = struct.unpack_from("<H", self.raw, p); p += 2
        if v & 0x8000:
            hi = v & 0x7fff
            lo, = struct.unpack_from("<H", self.raw, p); p += 2
            v = (hi << 16) | lo
        return v, p


def enc_str(s, utf8):
    if utf8:
        b = s.encode("utf-8")
        def l8(n):
            return bytes([(n >> 8) | 0x80, n & 0xff]) if n > 0x7f else bytes([n])
        return l8(len(s)) + l8(len(b)) + b + b"\x00"
    else:
        b = s.encode("utf-16-le")
        n = len(s)
        head = struct.pack("<H", n) if n <= 0x7fff else struct.pack("<HH", (n >> 16) | 0x8000, n & 0xffff)
        return head + b + b"\x00\x00"


def build_string_pool(strings, utf8):
    data = b""
    offsets = []
    for s in strings:
        offsets.append(len(data))
        data += enc_str(s, utf8)
    while len(data) % 4:
        data += b"\x00"
    count = len(strings)
    strings_start = 28 + count * 4
    body = struct.pack("<IIIII", count, 0, 0x100 if utf8 else 0, strings_start, 0)
    body += struct.pack("<%dI" % count, *offsets)
    body += data
    size = 8 + len(body)
    return struct.pack("<HHI", RES_STRING_POOL_TYPE, 28, size) + body


def chunk(ctype, body, header_extra=b""):
    header_size = 8 + len(header_extra)
    size = header_size + len(body)
    return struct.pack("<HHI", ctype, header_size, size) + header_extra + body


def start_elem(ns, name, attrs, line=0):
    # Start-element extension: lineNo(i), comment(i), ns(i), name(i), then
    # attributeStart(H)=0x14, attributeSize(H)=0x14, attributeCount(H), idIndex(H), classIndex(H), styleIndex(H).
    # attrs: list of (ns_idx, name_idx, raw_value_str_idx or -1, type, data)
    ext = struct.pack("<iii i HHHHHH", line, -1, ns, name, 0x14, 0x14, len(attrs), 0, 0, 0)
    body = b""
    for (a_ns, a_name, a_raw, a_type, a_data) in attrs:
        # ResXMLTree_attribute: ns(i), name(i), rawValue(i), then Res_value: size(H)=8, res0(B)=0, dataType(B), data(I)
        body += struct.pack("<iii HBBI", a_ns, a_name, a_raw, 0x08, 0, a_type, a_data & 0xFFFFFFFF)
    return chunk(RES_XML_START_ELEMENT_TYPE, ext + body)


def end_elem(ns, name, line=0):
    # End-element extension: lineNo(i), comment(i), ns(i), name(i)
    ext = struct.pack("<iiii", line, -1, ns, name)
    return chunk(RES_XML_END_ELEMENT_TYPE, ext)


def add_components(raw, pkg):
    magic, hsize, total = struct.unpack_from("<HHI", raw, 0)
    assert magic == RES_XML_TYPE, "not a binary AndroidManifest.xml"

    # locate chunks
    off = 8
    pool_off = resmap_off = resmap_end = None
    chunks = []
    while off < total:
        ctype, chsize, csize = struct.unpack_from("<HHI", raw, off)
        chunks.append((ctype, off, csize))
        if ctype == RES_STRING_POOL_TYPE and pool_off is None:
            pool_off = off
        elif ctype == RES_XML_RESOURCE_MAP_TYPE:
            resmap_off, resmap_end = off, off + csize
        off += csize

    pool = StringPool(raw, pool_off)
    strings = list(pool.strings)
    s_index = {s: i for i, s in enumerate(strings)}

    def want_str(s):
        if s not in s_index:
            s_index[s] = len(strings)
            strings.append(s)
        return s_index[s]

    # resource map: list of attribute ids, aligned with the first N pool strings (the attribute names)
    rm_count = (resmap_end - (resmap_off + 8)) // 4
    resmap = list(struct.unpack_from("<%dI" % rm_count, raw, resmap_off + 8))

    # Attribute names must sit at the pool index equal to their resource-map slot. The manifest already has
    # android:name/theme/exported/authorities etc. in most cases; add any missing ones at the end of BOTH the
    # attribute-name region and the resource map, keeping them in lockstep.
    def want_attr(attr_name):
        rid = ATTR[attr_name]
        if attr_name in s_index and s_index[attr_name] < len(resmap) and resmap[s_index[attr_name]] == rid:
            return s_index[attr_name]
        # append a new attribute-name string at index == len(resmap), extend resmap
        idx = len(resmap)
        # pad the string list so the new attribute name lands exactly at `idx`
        while len(strings) < idx:
            strings.append("")           # filler (unreferenced); keeps indices aligned
        if len(strings) == idx:
            strings.append(attr_name)
        else:
            strings[idx] = attr_name
        s_index[attr_name] = idx
        resmap.append(rid)
        return idx

    # strings we reference as values / element names
    want_str("application")  # usually present
    i_activity = want_str("activity")
    i_provider = want_str("provider")
    i_relay = want_str("com.bowlingplus.Pickers$Relay")
    i_fp = want_str("com.bowlingplus.BpFileProvider")
    i_auth = want_str(pkg + ".bowlingplus.fileprovider")
    i_empty = want_str("")

    # attribute-name indices (also grows resmap)
    a_name = want_attr("name")
    a_theme = want_attr("theme")
    a_exported = want_attr("exported")
    a_excl = want_attr("excludeFromRecents")
    a_auth = want_attr("authorities")
    a_grant = want_attr("grantUriPermissions")

    # find the android namespace uri index for attribute ns
    ns_uri_idx = s_index.get("http://schemas.android.com/apk/res/android", -1)

    def A(a_idx, type_, data, raw_idx=-1):
        return (ns_uri_idx, a_idx, raw_idx, type_, data)

    T, F = 0xFFFFFFFF, 0x00000000

    # <activity .../>
    act = start_elem(-1, i_activity, [
        A(a_name, TYPE_STRING, i_relay, i_relay),
        A(a_theme, TYPE_REFERENCE, THEME_TRANSLUCENT),
        A(a_exported, TYPE_INT_BOOLEAN, F),
        A(a_excl, TYPE_INT_BOOLEAN, T),
    ]) + end_elem(-1, i_activity)

    prov_attrs = [
        A(a_name, TYPE_STRING, i_fp, i_fp),
        A(a_auth, TYPE_STRING, i_auth, i_auth),
        A(a_exported, TYPE_INT_BOOLEAN, F),
        A(a_grant, TYPE_INT_BOOLEAN, T),
    ]
    prov = start_elem(-1, i_provider, prov_attrs) + end_elem(-1, i_provider)

    inject = act + prov

    # rebuild the file: header, new string pool, new resource map, then the element stream with `inject`
    # spliced just before the END of <application>.
    utf8 = pool.utf8
    new_pool = build_string_pool(strings, utf8)
    new_resmap = chunk(RES_XML_RESOURCE_MAP_TYPE, struct.pack("<%dI" % len(resmap), *resmap))

    # copy element chunks (everything after the resource map), splicing before </application>
    body = bytearray()
    i_app = s_index.get("application", -1)
    spliced = False
    p = resmap_end
    while p < total:
        ctype, chsize, csize = struct.unpack_from("<HHI", raw, p)
        if (not spliced and ctype == RES_XML_END_ELEMENT_TYPE):
            _, _, ns, nm = struct.unpack_from("<iiii", raw, p + 8)
            if nm == i_app:
                body += inject
                spliced = True
        body += raw[p:p + csize]
        p += csize
    if not spliced:
        raise RuntimeError("couldn't find </application> to splice into")

    new_body = new_pool + new_resmap + bytes(body)
    out = struct.pack("<HHI", RES_XML_TYPE, 8, 8 + len(new_body)) + new_body
    return out


if __name__ == "__main__":
    import sys
    raw = open(sys.argv[1], "rb").read()
    out = add_components(raw, sys.argv[2] if len(sys.argv) > 2 else "studio.wannaplay.bowlingjb")
    open(sys.argv[3], "wb").write(out)
    print("patched %d -> %d bytes" % (len(raw), len(out)))
