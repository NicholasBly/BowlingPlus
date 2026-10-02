#!/usr/bin/env python3
"""Put BowlingPlus.dylib inside an IPA so any signer (Signulous, Sideloadly, ...) installs it.

usage:  python3 inject_ipa.py  Game.ipa  BowlingPlus.dylib  Output.ipa
Only uses the Python standard library.
"""
import os
import plistlib
import struct
import sys
import zipfile

LC_LOAD_DYLIB = 0xC
LC_LOAD_WEAK_DYLIB = 0x80000018
LC_SEGMENT_64 = 0x19
MH_MAGIC_64 = 0xFEEDFACF
ZEROFILL_TYPES = (0x1, 0xC, 0x12)


def add_load_command(binary, dylib_path):
    """Append an LC_LOAD_DYLIB command in the free space after the existing load commands."""
    data = bytearray(binary)
    magic, _, _, _, ncmds, sizeofcmds = struct.unpack_from('<IiiIII', data, 0)
    if magic != MH_MAGIC_64:
        sys.exit('The app binary is not a thin 64-bit Mach-O file.')
    off, first_data = 32, len(data)
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<II', data, off)
        if cmd in (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB):
            name_off = struct.unpack_from('<I', data, off + 8)[0]
            name = bytes(data[off + name_off:off + cmdsize]).split(b'\0')[0].decode()
            if name == dylib_path:
                print('Load command already there - just updating the dylib.')
                return bytes(data)
        if cmd == LC_SEGMENT_64:
            nsects = struct.unpack_from('<I', data, off + 64)[0]
            for s in range(nsects):
                sect = off + 72 + s * 80
                sect_off = struct.unpack_from('<I', data, sect + 48)[0]
                sect_type = struct.unpack_from('<I', data, sect + 64)[0] & 0xFF
                if sect_off and sect_type not in ZEROFILL_TYPES:
                    first_data = min(first_data, sect_off)
        off += cmdsize
    path = dylib_path.encode() + b'\0'
    size = (24 + len(path) + 7) & ~7
    if off + size > first_data or any(data[off:off + size]):
        sys.exit('Not enough free space in the app binary header.')
    cmd = struct.pack('<IIIIII', LC_LOAD_DYLIB, size, 24, 2, 0x10000, 0x10000) + path
    data[off:off + size] = cmd + b'\0' * (size - len(cmd))
    struct.pack_into('<II', data, 16, ncmds + 1, sizeofcmds + size)
    return bytes(data)


def allow_120hz(data):
    """iPhones cap apps at 60 Hz unless Info.plist has CADisableMinimumFrameDurationOnPhone = YES
    (the original sets it to NO). This only allows higher rates; the game still picks its own
    frame rate unless BowlingPlus's 120 FPS mode is on."""
    fmt = plistlib.FMT_BINARY if data[:8] == b'bplist00' else plistlib.FMT_XML
    info = plistlib.loads(data)
    info['CADisableMinimumFrameDurationOnPhone'] = True
    # the original text is wrong (it says photo library); BowlingPlus uses the camera to scan oil pattern QR codes
    info['NSCameraUsageDescription'] = 'Scan a QR code to import a custom oil pattern.'
    return plistlib.dumps(info, fmt=fmt)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    ipa_in, dylib, ipa_out = sys.argv[1:]
    dylib_name = os.path.basename(dylib)
    with zipfile.ZipFile(ipa_in) as zin:
        app = next(n.split('/')[1] for n in zin.namelist()
                   if n.startswith('Payload/') and n.split('/')[1].endswith('.app'))
        prefix = 'Payload/%s/' % app
        exe = plistlib.loads(zin.read(prefix + 'Info.plist'))['CFBundleExecutable']
        target = prefix + 'Frameworks/' + dylib_name
        with zipfile.ZipFile(ipa_out, 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as zout:
            for info in zin.infolist():
                if info.filename == target:
                    continue  # replaced below
                data = zin.read(info)
                if info.filename == prefix + exe:
                    data = add_load_command(data, '@executable_path/Frameworks/' + dylib_name)
                elif info.filename == prefix + 'Info.plist':
                    data = allow_120hz(data)
                zout.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED)
            zi = zipfile.ZipInfo(target, date_time=(2026, 1, 1, 0, 0, 0))
            zi.create_system = 3
            zi.external_attr = 0o100755 << 16
            with open(dylib, 'rb') as f:
                zout.writestr(zi, f.read(), compress_type=zipfile.ZIP_DEFLATED)
    print('Done: %s - sign/install it with Signulous, Sideloadly, etc.' % ipa_out)


if __name__ == '__main__':
    main()
