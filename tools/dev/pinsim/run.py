#!/usr/bin/env python3
"""Pin-turn simulation: splices the LIVE pin-turn block out of android/native/Game.cpp (byte-identical to the block
in src/Game.mm) into a fake game (fake.h + scenario.h: kegels racked exactly like the game's
RunPsycsTest.UpdatePinPositions, Random.Range(0, 16) drawn through the game's engine-function pointer, throws that
knock pins over) and runs the checks in checks.inc.
Usage (from the repo root):  python3 tools/dev/pinsim/run.py"""
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, '../../../android/native/Game.cpp')).read()
a = src.index('// ---- pins keep their turn (Practice) ----')
b = src.index('// ---- your own pin image')
out = os.path.join(here, 'build_sim.cpp')
open(out, 'w').write('#include "fake.h"\n' + src[a:b] + '#include "scenario.h"\n' + open(os.path.join(here, 'checks.inc')).read())
exe = os.path.join(here, 'sim')
subprocess.run(['g++', '-std=c++17', '-O0', '-w', out, '-o', exe], check=True)
sys.exit(subprocess.run([exe]).returncode)
