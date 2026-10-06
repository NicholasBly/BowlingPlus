# Minimal IL2CPP global-metadata.dat reader: types with their fields and methods (names only).
# Usage: python3 il2cpp_meta.py Game.ipa_or_base.apk  (prints the metadata version and record sizes; import it for types())
import struct, sys, zipfile
def load(path_or_bytes):
    d = path_or_bytes
    sanity, ver = struct.unpack_from('<Ii', d, 0)
    assert sanity == 0xFAB11BAF, hex(sanity)
    H = lambda i: struct.unpack_from('<ii', d, 8 + 8 * i)       # (offset, size) pairs
    strOff, _ = H(2); methOff, methSize = H(5); fieldOff, fieldSize = H(11); typeOff, typeSize = H(19)
    def S(i):
        e = d.index(b'\0', strOff + i); return d[strOff + i:e].decode('utf-8', 'replace')
    return d, ver, S, (methOff, methSize), (fieldOff, fieldSize), (typeOff, typeSize)
def find_sizes(d, S, meth, field, typ):
    # try record sizes until names decode as identifiers
    def ok(name): return name and name.replace('_','a').replace('<','').replace('>','').replace('`','').replace('.','')[:1].isalpha()
    res = {}
    for label, (off, size), cands in (('type', typ, (88, 92, 96, 84, 80, 100, 104)), ('method', meth, (32, 36, 40, 28)), ('field', field, (12, 16))):
        for c in cands:
            if size % c: continue
            good = sum(ok(S(struct.unpack_from('<i', d, off + c * k)[0])) for k in range(0, min(400, size // c)) if 0 <= struct.unpack_from('<i', d, off + c * k)[0] < 50_000_000)
            if good > 380 or (size // c < 400 and good == size // c): res[label] = c; break
    return res
if __name__ == '__main__':
    z = zipfile.ZipFile(sys.argv[1]); name = [n for n in z.namelist() if n.endswith('global-metadata.dat')][0]
    d, ver, S, meth, field, typ = load(z.read(name))
    print('metadata version', ver, find_sizes(d, S, meth, field, typ))

def types(d, S, meth, field, typ, sizes):
    tsz, msz, fsz = sizes['type'], sizes['method'], sizes['field']
    out = []
    for k in range(typ[1] // tsz):
        o = typ[0] + k * tsz
        ints = struct.unpack_from('<16i', d, o)
        cnt = struct.unpack_from('<8H', d, o + 64)
        name, ns, parent = S(ints[0]), S(ints[1]), ints[4]
        fstart, mstart, nfields, nmeth = ints[8], ints[9], cnt[2], cnt[0]
        fields = [S(struct.unpack_from('<i', d, field[0] + (fstart + i) * fsz)[0]) for i in range(nfields)] if fstart >= 0 else []
        methods = []
        if mstart >= 0:
            for i in range(nmeth):
                mo = meth[0] + (mstart + i) * msz
                nm = S(struct.unpack_from('<i', d, mo)[0])
                pc = struct.unpack_from('<H', d, mo + msz - 2)[0]
                methods.append('%s(%d)' % (nm, pc))
        out.append({'name': name, 'ns': ns, 'fields': fields, 'methods': methods, 'parent': parent, 'idx': k})
    return out
