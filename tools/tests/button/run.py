#!/usr/bin/env python3
"""The on-screen menu button's spot: the live BFButtonSpot (src/MenuButton.mm) on iPhones with a Dynamic Island, a
notch or neither, and the live ButtonSpot.java on Android screens with a punch hole, a notch, a corner hole, a status
bar or none: always on screen, never over the island or cutout, a saved spot stays put, up to the top away from the
cutout, and held below the top over no more than the island's width.
Usage (from the repo root):  python3 tools/tests/button/run.py   (needs g++; javac and java for the Android half)"""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from common import ROOT, read, function, build_and_run

here = os.path.dirname(os.path.abspath(__file__))
text = ('#include <cstdio>\n#include <cmath>\ntypedef double CGFloat;\n#define MIN(a, b) ((a) < (b) ? (a) : (b))\n'
        '#define MAX(a, b) ((a) > (b) ? (a) : (b))\n' + function(read('src/MenuButton.mm'), 'static void BFButtonSpot(') +
        read(os.path.join(here, 'checks.inc')))
rc = build_and_run(text, 'buttonsim', here)
if not shutil.which('javac') or not shutil.which('java'):
    print('menu button, Android: skipped (no javac/java)')
    sys.exit(rc)
out = tempfile.mkdtemp()
try:
    r = subprocess.run(['javac', '-d', out, os.path.join(ROOT, 'android/java/com/bowlingplus/ButtonSpot.java'),
                        os.path.join(here, 'ButtonSpotTest.java')])
    rc2 = r.returncode or subprocess.run(['java', '-cp', out, 'com.bowlingplus.ButtonSpotTest']).returncode
finally:
    shutil.rmtree(out, ignore_errors=True)
sys.exit(rc or rc2)
