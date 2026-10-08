# Bowlscore: what the 25 % and 42-44 % figures measure

## The real-world number (USBC)

- **Source:** US Bowling Congress, bowl.com, "New COR device & spec suspension", January 2024. This is the only reference we used. We recorded it in `VERIFIED_NOTES.md` ("USBC reference").
- **Test:** Bowlscore, a fixed grid of 23 entry positions and 11 entry angles, with 10 shots per position. That's 253 cells and 2,530 shots per ball.
  - Offsets: 0 to 5.5 in, in 0.25 in steps (23 values).
  - Entry angles: 0 to 10 degrees, in 1 degree steps (11 values).
- **Result:** 41.9-44.3 % strikes over the grid, for balls with a coefficient of restitution (COR) of 0.668-0.800. USBC says COR "minimally" affects strikes.
- **What we don't have:** the per-shot data, or the exact figure for each ball. Only the range was published (as far as we recorded). So "42-44 %" in our docs means that published range.

A strike here means all ten pins are down on the first ball.

## The game's number (the model)

The same grid, run in `pinlab/` on NVIDIA PhysX 4.1 with the game's own pins, lane, rack and settings (see `PHYSICS_SETTINGS.md`):

- Offsets `0.00, 0.25, ... 5.50` in, angles `0 ... 10` degrees, the same as above.
- Ball (`ExpBowlscore` in `pinlab.cpp`): 6.80 kg (15 lb), radius 0.108 m, the game's ball material (bounce 0.65). It rolls without slipping in a straight line at 8.0 m/s, starting 1 m before the point where its centre would touch the head pin at the chosen offset and angle. It has no ramp, hook or spin beyond pure roll.
- Pins: the game's rack, each pin given a random turn from the shot's seed.
- Each shot is simulated for 4 seconds.
- Shots per cell: 4 for the numbers in `results/SUMMARY.md` (1,012 shots per setup). The harness's default is 2.
- Each shot's random seed is fixed (`77 + offset*1000 + angle*10 + shot`), so a rerun gives the same numbers.
- A strike is `down == 10` in `bowlscore_<config>.csv`.
- `results/summarize_bowlscore.py` builds the table in `results/SUMMARY.md` with 95 % intervals (Wilson).
- The runs in `results/` were made on 8 October 2026. They're within the study's error of its published numbers: see `results/RERUN_NOTE.md`.

## What the two numbers can and can't tell you

- They compare **strike rates over the same grid**, nothing more. The model's ball is a straight, pure-rolling ball. The real Bowlscore ball comes off a ramp, so its speed and rolling state at contact differ from the model's. A real bowler's release, rev rate and axis tilt are not modelled either.
- The model has no lane oil.
- Bowlscore strike rate is one number. Two setups can match it and still leave different pins. That's why the study also reports full-rack and pocket-throw results and single-pin leaves (`PIN_PHYSICS_STUDY.md`, "Full racks" and the double-rate section).
- USBC's balls had COR 0.668-0.800. The model's ball has COR set by the game's material, not measured. In the model, pin bounce barely changed strikes, as USBC found.

## Reproducing it

```
cd pinlab
./build_physx.sh                                     # PhysX 4.1 from GitHub, about 3 minutes
PINLAB_SHOTS=4 ./pinlab bowlscore scene.txt stock > ../results/bowlscore_stock.csv
# one CSV per setup in ../results, then:
cd ../results && python3 summarize_bowlscore.py > SUMMARY.md
```

Each setup takes about 2 to 10 minutes on one core, depending on the setup (the converged reference is the slowest, because it uses a 0.5 ms step).
