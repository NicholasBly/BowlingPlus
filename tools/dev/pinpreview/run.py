#!/usr/bin/env python3
"""Renders the pin library's 3D preview on the host: a wrap picture (2:1) goes through the app's own conversion
(src/PinWrap.h, with the game's template from src/PinGuide.h) and the app's own preview renderer (src/PinPreview.h).
Usage (from the repo root):  python3 tools/dev/pinpreview/run.py pins/BrunswickCrownMax.png out.png
Writes 8 frames (one turn) side by side. Text that reads correctly here reads correctly in the game."""
import os, re, subprocess, sys, tempfile
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.join(here, '../../..')
tmp = tempfile.mkdtemp()
exe = os.path.join(tmp, 'pv')
subprocess.run(['g++', '-O2', '-std=c++17', '-w', '-I' + os.path.join(root, 'src'), os.path.join(here, 'pv.cpp'), '-o', exe], check=True)
h = open(os.path.join(root, 'src/PinGuide.h')).read()
i = h.index('kBPPinTemplatePNG[] = {'); j = h.index('};', i)
open(os.path.join(tmp, 't.png'), 'wb').write(bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', h[i:j])))
side = 1024
t = Image.new('RGBA', (side, side), (255, 255, 255, 255)); t.alpha_composite(Image.open(os.path.join(tmp, 't.png')).convert('RGBA').resize((side, side)))
open(os.path.join(tmp, 't.rgba'), 'wb').write(t.tobytes())
ww, wh = 2048, 1024
w = Image.new('RGBA', (ww, wh), (255, 255, 255, 255)); w.alpha_composite(Image.open(sys.argv[1]).convert('RGBA').resize((ww, wh)))
open(os.path.join(tmp, 'w.rgba'), 'wb').write(w.tobytes())
W, H, N = 180, 300, 8
subprocess.run([exe, os.path.join(tmp, 'w.rgba'), str(ww), str(wh), os.path.join(tmp, 't.rgba'), str(side), os.path.join(tmp, 'f'), str(N), str(W), str(H)], check=True)
strip = Image.new('RGBA', (W * N, H), (40, 44, 52, 255))
for k in range(N): strip.alpha_composite(Image.frombytes('RGBA', (W, H), open(os.path.join(tmp, 'f%02d.rgba' % k), 'rb').read()), (k * W, 0))
strip.save(sys.argv[2]); print('wrote', sys.argv[2])
