# Port: BowlingPlus features for the official game

Everything here is for porting features from BowlingPlus into Bowling by Jason Belmonte. It's based on BowlingPlus 1.7.2 (GitHub `main`, commit `7304f4b`).

Read in this order:

1. **`PORTING_GUIDE.md`**: every feature, its effort and gate, Practice gating, and how to avoid applying a fix twice.
2. **`features/120-fps.md`**: 120 FPS mode. This describes how BowlingPlus does it at runtime; the guide has the native version.
3. **`features/ipv6-loading-fix.md`**: the IPv6 loading fix and the loading watchdogs, as BowlingPlus runs them. The guide has the native version.
4. **`pin-physics/`**: the pin physics study. Start with `BOWLSCORE_REFERENCE.md`, which says what the 25 % and 42-44 % figures measure.

## Folder layout

```
Port/
  README.md                      this file
  PORTING_GUIDE.md               the guide
  features/
    120-fps.md
    ipv6-loading-fix.md
  pin-physics/
    PIN_PHYSICS_STUDY.md         the study (results and method)
    PHYSICS_SETTINGS.md          the game's exact pin, ball, lane and step values
    BOWLSCORE_REFERENCE.md       the USBC reference, the model's grid, what the numbers mean
    ENGINE_STEP.md               how the double physics rate changes the engine's step
    docs/                        the two charts used in the study
    pinlab/                      the PhysX 4.1 harness (build_physx.sh, pinlab.cpp, scene.txt)
    results/                     Bowlscore runs (CSV), SUMMARY.md, RERUN_NOTE.md, summarize_bowlscore.py
```

## What's not in here

- No game files, IPAs or APKs. You have those.
- No signing keys or passwords.
- No BowlingPlus source beyond what's cited. The full source is in the BowlingPlus repository (`src/`, `android/`).

## Reproducing the pin study

The commands in `pin-physics/BOWLSCORE_REFERENCE.md` run from `Port/pin-physics/`. `pinlab/build_physx.sh` downloads PhysX 4.1 from NVIDIA's GitHub and needs `git`, `cmake` and `clang`. It takes a few minutes on one core.

## Contact

Nick Bly, BowlingPlus (github.com/NicholasBly/BowlingPlus).
