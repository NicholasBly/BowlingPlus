#!/usr/bin/env python3
"""Barstools: runs the live RoStoolBounce and RoStoolsTick from android/native/Game.cpp on every stool layout with a
fake ball the engine moves by its velocity: 30-144 Hz, 4-10.5 m/s, straight and hooking both ways, aimed across each
stool. Every ball that comes within reach is bounced, never goes on into the stool, never heads back up the lane or
speeds up, and ends in the gutter before the pins; a ball that stays clear is never bounced.
Usage (from the repo root):  python3 tools/tests/stools/run.py"""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from common import read, region, function, build_and_run, ANDROID

here = os.path.dirname(os.path.abspath(__file__))
src = read(ANDROID)
reach = re.search(r'static const float kStoolReach = ([\d.]+)f;', src).group(1)
code = (function(src, 'static float SegDist2(') + 'static const float kStoolReach = ' + reach + 'f;\n' +
        region(src, '// the ball meets a barstool (it has no collider', 'static void RoSnapStart();'))
text = '#include "fake.h"\n' + code + read(os.path.join(here, 'checks.inc'))
sys.exit(build_and_run(text, 'stoolsim', here))
