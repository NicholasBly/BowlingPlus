#!/usr/bin/env python3
"""Obstacle racks: runs the live "new layout only for a new rack" code (RoRackDecide, RoFallTick, RoRackTick) from
android/native/Game.cpp against a fake game (fake.h: ten pins, frames with the 10th's bonus balls, the locations
around a throw) through whole games: strikes, open frames, redos (a ball thrown off the lane), gutters, the 10th
frame, scene changes, restarts, a late frame number, and the fields not found.
Usage (from the repo root):  python3 tools/tests/rack/run.py"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from common import read, region, build_and_run, ANDROID

here = os.path.dirname(os.path.abspath(__file__))
src = read(ANDROID)
code = region(src, '// A new layout only for a new rack', 'static void RampsTick() {')
text = '#include "fake.h"\n' + code + read(os.path.join(here, 'scenario.inc'))
sys.exit(build_and_run(text, 'racksim', here))
