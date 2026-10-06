#!/usr/bin/env python3
"""Checks BowlingPlus's pin-turn mechanism (1.6.6+) against the real game binaries, without a device.

1. Runs the game's own Random.Range(int, int) machine code in an ARM64 emulator (unicorn), entered the way
   RunPsycsTest.UpdatePinPositions enters it, with the engine-function pointer aimed at a stub. The stub must
   be reached with x30 = UpdatePinPositions' return address, w0 = 0, w1 = 16, and sp / x29 restored.
2. Compiles the shipped decoders (RandSlotIn, FindTurnSites) straight out of android/native/Game.cpp, maps
   each binary at its own layout (base + VA) and checks they find the right pointer and exactly one call site.

Usage (from the repo root):
  pip install unicorn            # (capstone not needed)
  python3 tools/dev/pinhook/check.py --so path/to/libil2cpp.so --ios path/to/UnityFramework
Either binary can be left out. The addresses below are for game 1.907 (Android build 1597); after a game
update, find the new ones with Il2CppDumper (script.json: RunPsycsTest$$UpdatePinPositions,
UnityEngine.Random$$Range, UnityEngine.Random$$RandomRangeInt) and the slot from Range's ADRP/LDR, and pass
them with --android-addrs / --ios-addrs (upp,range,rangeint,slot,site), site = the BL's address + 4.
"""
import argparse, os, struct, subprocess, sys, tempfile

ANDROID = dict(upp=0x1791964, range=0x2D4FC98, rangeint=0x2D4FCDC, slot=0x33DE700, site=0x1791D34)
IOS = dict(upp=0x1EBCFD4, range=0x384DBDC, rangeint=0x384DC40, slot=0x4E39030, site=0x1EBD390)


def elf_segments(data):
    assert data[:4] == b'\x7fELF' and data[4] == 2, 'not a 64-bit ELF'
    phoff = struct.unpack_from('<Q', data, 0x20)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    segs = []
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz = struct.unpack_from('<IIQQQQ', data, phoff + i * phentsize)
        if p_type == 1:   # PT_LOAD
            segs.append((p_vaddr, p_offset, p_filesz))
    return segs


def macho_segments(data):
    magic, _, _, _, ncmds = struct.unpack_from('<IiiII', data, 0)
    assert magic == 0xFEEDFACF, 'not a thin 64-bit Mach-O'
    off, segs = 32, []
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, off)
        if cmd == 0x19:   # LC_SEGMENT_64
            name = data[off + 8:off + 24].split(b'\0')[0]
            vmaddr, _, fileoff, filesize = struct.unpack_from('<QQQQ', data, off + 24)
            if name != b'__PAGEZERO' and name != b'__LINKEDIT' and filesize:
                segs.append((vmaddr, fileoff, filesize))
        off += size
    return segs


def va2off(segs, va):
    for v, o, sz in segs:
        if v <= va < v + sz:
            return va - v + o
    raise ValueError(hex(va))


def emulate(name, data, segs, a):
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UcError
    from unicorn.arm64_const import UC_ARM64_REG_SP, UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X29, UC_ARM64_REG_X30
    ok_all = True
    for entry in ('range', 'rangeint'):
        mu = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        page = a[entry] & ~0xFFF
        o = va2off(segs, page)
        mu.mem_map(page, 0x2000)
        mu.mem_write(page, data[o:o + 0x2000])
        mu.mem_map(a['slot'] & ~0xFFF, 0x1000)
        stub, stack = 0x7000000, 0x8000000
        mu.mem_map(stub, 0x1000)
        mu.mem_write(stub, b'\x00\x00\x20\xd4')
        mu.mem_write(a['slot'], stub.to_bytes(8, 'little'))
        mu.mem_map(stack, 0x10000)
        mu.reg_write(UC_ARM64_REG_SP, stack + 0x8000)
        mu.reg_write(UC_ARM64_REG_X0, 0)
        mu.reg_write(UC_ARM64_REG_X1, 16)
        mu.reg_write(UC_ARM64_REG_X29, 0x1234)
        mu.reg_write(UC_ARM64_REG_X30, a['site'])
        seen = {}

        def hook(uc, addr, size, ud):
            if addr == stub:
                seen.update(x30=uc.reg_read(UC_ARM64_REG_X30), x0=uc.reg_read(UC_ARM64_REG_X0), x1=uc.reg_read(UC_ARM64_REG_X1),
                            sp=uc.reg_read(UC_ARM64_REG_SP), x29=uc.reg_read(UC_ARM64_REG_X29))
                uc.emu_stop()
        mu.hook_add(UC_HOOK_CODE, hook)
        try:
            mu.emu_start(a[entry], 0, count=200)
        except UcError as e:
            seen['error'] = str(e)
        ok = (seen.get('x30') == a['site'] and seen.get('x0') == 0 and seen.get('x1') == 16 and
              seen.get('sp') == stack + 0x8000 and seen.get('x29') == 0x1234)
        print('  %-8s emulated %-15s -> stub with x30=%s (want %#x), w0=%s w1=%s, sp/x29 restored: %s  %s' % (
            name, entry, hex(seen['x30']) if 'x30' in seen else None, a['site'], seen.get('x0'), seen.get('x1'),
            seen.get('sp') == stack + 0x8000 and seen.get('x29') == 0x1234, 'PASS' if ok else 'FAIL'))
        ok_all &= ok
    return ok_all


HARNESS = r'''
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <fstream>
#include <iterator>
#include <vector>
%(decoders)s
int main(int argc, char **argv) {
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<char> d((std::istreambuf_iterator<char>(f)), {});
    uint8_t *b = (uint8_t *)mmap(nullptr, %(span)d, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    %(copies)s
    int fails = 0;
    void *slot = RandSlotIn((const uint32_t *)(b + %(rangeint)d));
    if (slot != b + %(slot)d) { printf("    RandomRangeInt -> slot: FAIL\n"); fails++; } else printf("    RandomRangeInt -> slot: PASS\n");
    if (RandSlotIn((const uint32_t *)(b + %(range)d)) != b + %(slot)d) { printf("    Range(int,int) -> same slot: FAIL\n"); fails++; } else printf("    Range(int,int) -> same slot: PASS\n");
    if (RandSlotIn((const uint32_t *)(b + %(upp)d)) != nullptr) { printf("    UpdatePinPositions is not a slot loader: FAIL\n"); fails++; } else printf("    UpdatePinPositions is not a slot loader: PASS\n");
    uintptr_t sites[4];
    int n = FindTurnSites((const uint32_t *)(b + %(upp)d), slot, sites, 4);
    bool ok = n == 1 && sites[0] == (uintptr_t)(b + %(site)d);
    printf("    call sites found: %%d%%s -> %%s\n", n, n ? "" : "", ok ? "PASS" : "FAIL");
    if (!ok) fails++;
    return fails != 0;
}
'''


def decoders_check(name, path, segs, a, decoders):
    span = max(v + sz for v, o, sz in segs) + 0x100000
    copies = ''.join('{ size_t n = %d; if ((size_t)%d + n > d.size()) n = d.size() - %d; memcpy(b + %d, d.data() + %d, n); }\n    ' %
                     (sz, o, o, v, o) for v, o, sz in segs)
    src = HARNESS % dict(decoders=decoders, span=span, copies=copies, **a)
    tmp = tempfile.mkdtemp()
    cpp, exe = os.path.join(tmp, 'h.cpp'), os.path.join(tmp, 'h')
    open(cpp, 'w').write(src)
    subprocess.run(['g++', '-std=c++17', '-O1', '-w', cpp, '-o', exe], check=True)
    print('  %s decoders (from android/native/Game.cpp):' % name)
    return subprocess.run([exe, path]).returncode == 0


def addrs(text, default):
    if not text:
        return default
    vals = [int(x, 16) for x in text.split(',')]
    return dict(zip(('upp', 'range', 'rangeint', 'slot', 'site'), vals))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--so', help="the game's lib/arm64-v8a/libil2cpp.so")
    ap.add_argument('--ios', help="the game's Frameworks/UnityFramework.framework/UnityFramework (decrypted)")
    ap.add_argument('--android-addrs')
    ap.add_argument('--ios-addrs')
    args = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    game = open(os.path.join(here, '../../../android/native/Game.cpp')).read()
    decoders = game[game.index('static void *RandSlotIn(const uint32_t *code) {'):game.index('static void TurnHookInstall() {')]
    ok = True
    for name, path, kind, default, override in (('Android', args.so, 'elf', ANDROID, args.android_addrs),
                                                 ('iOS', args.ios, 'macho', IOS, args.ios_addrs)):
        if not path:
            continue
        data = open(path, 'rb').read()
        segs = elf_segments(data) if kind == 'elf' else macho_segments(data)
        a = addrs(override, default)
        print(name + ':')
        try:
            ok &= emulate(name, data, segs, a)
        except ImportError:
            print('  (pip install unicorn for the emulator check)')
        ok &= decoders_check(name, path, segs, a, decoders)
    if not (args.so or args.ios):
        ap.error('give --so and/or --ios')
    print('all passed' if ok else 'FAILED')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
