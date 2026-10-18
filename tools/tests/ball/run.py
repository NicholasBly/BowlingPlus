#!/usr/bin/env python3
"""Loft and landing: runs the live LoftVz (the upward speed for a loft) and DampBounced (the damper's bounce test) from
android/native/Game.cpp. Every loft from 1 ft to the slider's top at 4-11 m/s, let go 0-40 cm above the lane, must land
within 1 cm of the setting (or be held by the 1 m cap); every landing at 30-144 Hz, at any phase, must be caught before
the bounce's top; a rolling ball must never be touched.
Usage (from the repo root):  python3 tools/tests/ball/run.py"""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from common import read, function, build_and_run, ANDROID

here = os.path.dirname(os.path.abspath(__file__))
src = read(ANDROID)
loft = re.search(r'#define BF_MAX_LOFT\s+(\d+)', read('android/native/BFShared.h')).group(1)
text = ('#include <cstdio>\n#include <cmath>\n#define BF_MAX_LOFT ' + loft + '\n' +
        function(src, 'static float LoftVz(') + function(src, 'static bool DampBounced(') + read(os.path.join(here, 'checks.inc')))
sys.exit(build_and_run(text, 'ballsim', here))
