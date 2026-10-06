# Usage: python3 dex_classes.py <folder with the game's classes*.dex>  -> classes.pkl (class -> (super, methods, fields))
# Minimal DEX reader: which methods/fields does a class DEFINE (class_data), across all classes*.dex.
import struct, sys, glob
def uleb(b, o):
    r = s = 0
    while True:
        x = b[o]; o += 1; r |= (x & 0x7f) << s; s += 7
        if x < 0x80: return r, o
def load(path):
    b = open(path, 'rb').read()
    def hdr(off): return struct.unpack_from('<II', b, off)
    sz, so = hdr(0x38); tz, to = hdr(0x40); pz, po = hdr(0x48); fz, fo = hdr(0x50); mz, mo = hdr(0x58); cz, co = hdr(0x60)
    def string(i):
        off = struct.unpack_from('<I', b, so + 4*i)[0]; _, off = uleb(b, off)
        e = b.index(0, off); return b[off:e].decode('utf-8', 'replace')
    types = [string(struct.unpack_from('<I', b, to + 4*i)[0]) for i in range(tz)]
    def proto(i):
        shorty, ret, poff = struct.unpack_from('<III', b, po + 12*i)
        params = []
        if poff:
            n = struct.unpack_from('<I', b, poff)[0]
            params = [types[struct.unpack_from('<H', b, poff + 4 + 2*k)[0]] for k in range(n)]
        return '(' + ''.join(params) + ')' + types[ret]
    def meth(i):
        c, p, n = struct.unpack_from('<HHI', b, mo + 8*i); return types[c], string(n) + proto(p)
    def field(i):
        c, t, n = struct.unpack_from('<HHI', b, fo + 8*i); return types[c], string(n) + ':' + types[t]
    out = {}
    for k in range(cz):
        cls, acc, sup, ifs, src, ann, cdata, sv = struct.unpack_from('<8I', b, co + 32*k)
        name = types[cls]; ms, fs = [], []
        if cdata:
            o = cdata; sf, inf, dm, vm = [0]*4
            sf, o = uleb(b, o); inf, o = uleb(b, o); dm, o = uleb(b, o); vm, o = uleb(b, o)
            for group in (sf, inf):
                idx = 0
                for _ in range(group):
                    d, o = uleb(b, o); idx += d; _, o = uleb(b, o); fs.append(field(idx)[1])
            for group in (dm, vm):
                idx = 0
                for _ in range(group):
                    d, o = uleb(b, o); idx += d; _, o = uleb(b, o); _, o = uleb(b, o); ms.append(meth(idx)[1])
        out[name] = (types[sup] if sup != 0xffffffff else None, ms, fs)
    return out
if __name__ == '__main__':
    import pickle
    allc = {}
    for p in sorted(glob.glob(sys.argv[1] + '/classes*.dex')): allc.update(load(p))
    pickle.dump(allc, open('classes.pkl', 'wb'))   # names per class, for has-this-method checks
    print(len(allc), 'classes')
