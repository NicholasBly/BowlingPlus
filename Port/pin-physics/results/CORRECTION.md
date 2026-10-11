# Correction: the materials' combine mode (11 October 2026)

## What was wrong

The pin study (BowlingPlus 1.7.0, rerun on 8 October 2026) built the game's physics materials with the combine mode **Minimum**. The game's materials store combine mode **1**, and in Unity's `PhysicsMaterialCombine` enum 1 is **Multiply** (the game's own metadata: Average 0, Multiply 1, Minimum 2, Maximum 3). The serialized value is that enum, so every one of the game's materials is Multiply, except the cushions' and the pit floor's bounce (2, Minimum) and the kickbacks' bounce (3, Maximum).

Of two touching colliders, the one with the higher-priority mode decides (PhysX and Unity: average < minimum < multiply < maximum). So in the game:

| Contact | Modelled before (Minimum) | The game (Multiply) |
|---|---|---|
| Pin on pin, friction sliding / starting | 0.5 / 0.3 | 0.25 / 0.09 |
| Pin on pin, bounce | 0.65 | 0.42 |
| Pin on deck, friction | 0.5 / 0.3 | 0.30 / 0.27 |
| Pin on ball, friction | 0.3 / 0.3 | 0.15 / 0.09 |
| Ball on pin, bounce | 0.65 | 0.42 |

A pin friction set at runtime (BowlingPlus's "Realistic pin physics") is multiplied the same way: 0.25 on the pin material is 0.0625 pin on pin.

## What changed in the results

Every CSV in this folder was made again with each material's own combine mode (`pinlab.cpp` reads it from `scene.txt` now). 1,012 Bowlscore shots per setup, ±3 points at 95 %:

| Setup | Before | Now |
|---|---|---|
| Game as shipped | 25.1-27.0 % | **46.5 %** |
| Converged reference | 34.2-35.5 % | 34.4 % |
| Pin friction 0.25, game's step | 35.0-37.2 % | 38.4 % |
| Pin friction 0.40, game's step | (not run) | 44.0 % |
| Double rate, game's friction | 24.7 % | 50.0 % |
| Double rate, pin friction 0.25 | 40.4-40.5 % | 44.0 % |
| Double rate, pin friction 0.35 | 41.7-41.8 % | 48.8 % |

Real pins: 41.9-44.3 % (USBC).

- **The main claim is reversed.** The game's own pins don't strike far less than real pins; on this test they strike a little more (46.5 %).
- **BowlingPlus 1.7.0-1.7.4's friction 0.25 at the game's step took it the wrong way** (38.4 %). 1.7.5 recommends 0.40 at the game's step and 0.25 with the double rate; both come out at 44.0 %.
- **Entry angle**: the earlier "angle doesn't matter in the game, matters with friction 0.25" doesn't hold. In every setup strikes peak at 4-7° and fall at 8-10°.
- **Restitution**: it now matters (pin bounce 0.75 with the double rate: +6 points). Still not settable at runtime.
- **Still true**: the 7.5 ms step puts the ball 25 mm into a pin on average before the hit is resolved; halving the step roughly halves the ball-pin and pin-pin errors; larger contact offsets, more iterations and speculative collisions don't help (`PIN_PHYSICS_STUDY.md`, results 1 and 2).

`SUMMARY.md` is the full table; `make_charts.py` draws the study's two charts from these files.
