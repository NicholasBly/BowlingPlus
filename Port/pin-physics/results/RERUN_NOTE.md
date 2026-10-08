# Bowlscore rerun: how it compares with the published study

Date of the rerun: 8 October 2026. Harness: `pinlab/pinlab.cpp` as in this folder, PhysX 4.1 built from NVIDIA's GitHub, 4 shots per grid cell (1,012 shots per setup), seed as described in `BOWLSCORE_REFERENCE.md`.

The runs in this folder were made again from the harness and scene in this folder, because the first set of CSVs was lost when the working environment was reset. The numbers below are from the new runs.

## Published study vs. rerun

| Setup (`config`) | Study (`PIN_PHYSICS_STUDY.md`) | Rerun, 1,012 shots (95 % interval) | Difference |
|---|---|---|---|
| Game as shipped (`stock`) | 25.1 % | 27.0 % (±2.7) | +1.9 |
| Converged reference (`ref`) | 34.2 % | 35.5 % (±2.9) | +1.3 |
| Pin friction 0.25, game's step (`stock_pf025`) | 37.2 % | 35.0 % (±2.9) | −2.2 |
| Pin friction 0.30, game's step (`stock_pf03`) | 37.0 % | 36.8 % (±3.0) | −0.2 |
| Double rate, pin friction 0.25 (`dt375_pf025`) | 40.5 % | 40.4 % (±3.0) | −0.1 |
| Double rate, pin friction 0.30 (`dt375_pf03`) | 38.5 % | 37.5 % (±3.0) | −1.0 |
| Double rate, pin friction 0.35 (`dt375_pf035`) | 41.7 % | 41.8 % (±3.0) | +0.1 |
| Double rate, pin friction 0.40 (`dt375_pf04`) | 37.5 % | 36.9 % (±3.0) | −0.6 |

Every rerun is within the study's own sampling error. The study's first table doesn't record how many shots per cell it used. The harness default is 2 (506 shots), so that table's error is wider (about ±4 points). The study's double-rate table used 1,012 shots.

## What holds

- The game as shipped is about 25 to 27 % on the grid. Real pins are 41.9 to 44.3 % (USBC).
- Pin friction 0.25 at the game's step lifts that to about 35 to 37 %.
- The double rate with friction 0.25 reaches about 40 %.
- Friction 0.35 with the double rate gives 41.8 %, a little closer on strike rate. But its entry-angle effect is much weaker (40.8 % at 0-3 degrees against 43.5 % at 6-10, against 34.0 % and 43.5 % at 0.25), and the study found the 5 pin then becomes the most common single-pin leave. Friction 0.20 overshoots (49.7 %). That's why 0.25 was chosen: it keeps the entry-angle effect and the 10 pin as the usual leave, not because it's the closest strike rate.
- Entry angle matters more with friction 0.25 than with the game's own friction. Compare the "0-3 degrees" and "6-10 degrees" columns in `SUMMARY.md`.

## What this doesn't settle

- None of these is a fit to real pins. The 41.9 to 44.3 % figure is a range over balls with different COR, and the model's ball is a straight pure-rolling ball (see `BOWLSCORE_REFERENCE.md`).
- The `ref` run uses a 0.5 ms step and 20 solver iterations. The game can't reach those settings (`PIN_PHYSICS_STUDY.md`), so it's a reference for what the step does, not a setting to ship.
- The `leave` column is a bitmask: bit `i` is set when pin `i` is still standing, with pin 0 the head pin.

## Changes to the study's wording

No change is needed in the study's conclusions. The published figures should be quoted as ranges: "about 25 to 27 %" for the game as shipped, "about 35 to 37 %" for friction 0.25 at the game's step, and "about 40 %" for the double rate with friction 0.25.
