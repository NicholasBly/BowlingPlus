#!/usr/bin/env python3
"""iOS can't be compiled without the iOS SDK, and the 1.6.1 iOS build broke on a function used before its
declaration (MenuButton.mm). This scans src/*.mm + Tweak.xm for static functions used before they're
declared. Known false positive: Backup.mm crc32 (a class member; C++ allows that).
Usage (from the repo root):  python3 tools/dev/ios_order_scan.py"""
import re, glob
issues = 0
for p in sorted(glob.glob('src/*.mm')) + ['Tweak.xm']:
    code = [re.sub(r'//.*', '', l) for l in open(p, errors='replace').read().split('\n')]
    defs, protos = {}, {}
    for i, l in enumerate(code):
        m = re.match(r'\s*static\s+(?:inline\s+)?[A-Za-z_][\w\s\*<>,:]*?\b([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*(\{|;)', l)
        if m: (defs if m.group(3) == '{' else protos).setdefault(m.group(1), i)
    for name, d in defs.items():
        first = min(d, protos.get(name, 10**9))
        for i, l in enumerate(code[:first]):
            if re.search(r'(?<![\w.])' + re.escape(name) + r'\s*\(', l):
                print(f'{p}:{i+1} {name} used before its declaration (line {d+1})'); issues += 1; break
print('problems:', issues, '(Backup.mm crc32 is a known false positive)')
