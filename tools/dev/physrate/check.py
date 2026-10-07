#!/usr/bin/env python3
"""Checks the double physics rate's engine lookup against the game's own binaries: compiles RateFindGetter (from
src/Game.mm) and runs it on the native Time.get_fixedDeltaTime of libunity.so (Android) and UnityFramework (iOS), found
through the engine's own registration (the icall name's adrp/add, then the function in x1). Also checks it refuses a
lookalike (Time.set_timeScale: a call, but no step reads), and prints the TimeManager layout the feature relies on.
Usage: python3 tools/dev/physrate/check.py --so libunity.so --ios UnityFramework     (pip install pyelftools capstone numpy)"""
import argparse, os, struct, subprocess, sys, tempfile, numpy as np
ap = argparse.ArgumentParser(); ap.add_argument('--so'); ap.add_argument('--ios'); a = ap.parse_args()
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../..')
src = open(os.path.join(root, 'src/Game.mm')).read()
i = src.index('static const uint8_t *RateFindGetter('); j = src.index('\n}\n', i) + 3
tmp = tempfile.mkdtemp()
open(os.path.join(tmp, 't.cpp'), 'w').write('#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n#include <cstring>\n' + src[i:j] + r'''
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = (uint8_t *)malloc(n); fread(d, 1, n, f); fclose(f);
    long off = strtol(argv[2], 0, 16); const char *why = "";
    const uint8_t *t = RateFindGetter((const uint32_t *)(d + off), &why);
    printf("%ld %s\n", t ? (long)(t - d) - off : 0L, why);
}
''')
exe = os.path.join(tmp, 't'); subprocess.run(['g++', '-std=c++17', '-w', os.path.join(tmp, 't.cpp'), '-o', exe], check=True)
def scan_ref(w, tva, target):                       # adrp+add producing target
    isadrp = (w & 0x9F000000) == 0x90000000; idx = np.nonzero(isadrp)[0]; ww = w[idx].astype(np.int64)
    imm = (((ww >> 5) & 0x7FFFF) << 2) | ((ww >> 29) & 3); imm = np.where(imm >= (1 << 20), imm - (1 << 21), imm)
    page = ((tva + idx * 4) & ~0xFFF) + (imm << 12)
    for c in idx[page == (target & ~0xFFF)]:
        for k in range(1, 4):
            x = int(w[c + k])
            if (x & 0xFFC00000) == 0x91000000 and ((x >> 10) & 0xFFF) == (target & 0xFFF): return int(c)
def x1_func(w, tva, c):                             # the function put in x1 after the name (adr x1 / adrp+add x1)
    for k in range(c + 2, c + 6):
        x = int(w[k]); pc = tva + k * 4
        if (x & 0x9F00001F) == 0x10000001:          # ADR x1
            imm = (((x >> 5) & 0x7FFFF) << 2) | ((x >> 29) & 3); imm = imm - (1 << 21) if imm >= (1 << 20) else imm; return pc + imm
        if (x & 0x9F00001F) == 0x90000001:          # ADRP x1 ; ADD x1, x1, #
            imm = (((x >> 5) & 0x7FFFF) << 2) | ((x >> 29) & 3); imm = imm - (1 << 21) if imm >= (1 << 20) else imm
            y = int(w[k + 1]); return (pc & ~0xFFF) + (imm << 12) + ((y >> 10) & 0xFFF)
def check(label, data, text_va, text_off, text_size, va2off, off2va):
    w = np.frombuffer(data[text_off:text_off + text_size // 4 * 4], dtype='<u4')
    results = []
    for name, expect_ok in ((b'UnityEngine.Time::get_fixedDeltaTime', True), (b'UnityEngine.Time::set_timeScale', False)):
        s = data.find(name + b'\0'); c = scan_ref(w, text_va, off2va(s)); fn = x1_func(w, text_va, c)
        out = subprocess.run([exe, path_of[label], hex(va2off(fn))], capture_output=True, text=True).stdout.split(' ', 1)
        rel = int(out[0]); why = out[1].strip()
        ok = (rel != 0) == expect_ok
        tgt = fn + rel if rel else 0
        results.append(ok)
        print('%-8s %-40s native 0x%x -> %s  %s' % (label, name.decode(), fn, ('TimeManager getter 0x%x' % tgt) if rel else ('refused (%s)' % why), 'PASS' if ok else 'FAIL'))
    return all(results)
path_of = {}
allok = True
if a.so:
    from elftools.elf.elffile import ELFFile
    path_of['Android'] = a.so; d = open(a.so, 'rb').read(); e = ELFFile(open(a.so, 'rb')); t = e.get_section_by_name('.text')
    segs = [s for s in e.iter_segments() if s['p_type'] == 'PT_LOAD']
    va2off = lambda v: next(v - s['p_vaddr'] + s['p_offset'] for s in segs if s['p_vaddr'] <= v < s['p_vaddr'] + s['p_filesz'])
    off2va = lambda o: next(o - s['p_offset'] + s['p_vaddr'] for s in segs if s['p_offset'] <= o < s['p_offset'] + s['p_filesz'])
    allok &= check('Android', d, t['sh_addr'], t['sh_offset'], t['sh_size'], va2off, off2va)
if a.ios:
    path_of['iOS'] = a.ios; d = open(a.ios, 'rb').read(); ncmds = struct.unpack_from('<I', d, 16)[0]; off = 32; segs = []; text = None
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', d, off)
        if cmd == 0x19:
            vm, vs, fo, fs = struct.unpack_from('<QQQQ', d, off + 24); segs.append((vm, fo, fs))
            for k in range(struct.unpack_from('<I', d, off + 64)[0]):
                so = off + 72 + k * 80
                if d[so:so + 16].rstrip(b'\0') == b'__text': text = (struct.unpack_from('<Q', d, so + 32)[0], struct.unpack_from('<Q', d, so + 40)[0], struct.unpack_from('<I', d, so + 48)[0])
        off += size
    va2off = lambda v: next(v - vm + fo for vm, fo, fs in segs if vm <= v < vm + fs)
    off2va = lambda o: next(o - fo + vm for vm, fo, fs in segs if fo <= o < fo + fs)
    allok &= check('iOS', d, text[0], text[2], text[1], va2off, off2va)
print('TimeManager layout used: step = count(int64 +0x50) x den(u32 +0x5c) / num(u32 +0x58); copy at +0x70 (16 bytes); 1/step float at +0x84')
print('all passed' if allok else 'FAILED'); sys.exit(0 if allok else 1)
