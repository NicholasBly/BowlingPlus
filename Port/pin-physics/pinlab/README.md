# pinlab

The game's pin physics rebuilt on NVIDIA PhysX 4.1 (the engine Unity 6 embeds), with the game's own pins, materials,
lane, rack and physics settings. Used for `PIN_PHYSICS_STUDY.md`.

- `build_physx.sh`: builds PhysX 4.1 from GitHub (static, Linux, clang) and `pinlab`.
- `pinlab.cpp`: the harness. Experiments: `rest`, `ballpin`, `pinpin`, `rack` (`PINLAB_N` throws), `bowlscore` (USBC's 23 offsets x 11 angles; `PINLAB_SHOTS`, `PINLAB_V`), `ramp` (BowlingPlus 1.7.5's ramps: `PINLAB_RAMPS`, `PINLAB_SPEEDS`, `PINLAB_AIM`, `PINLAB_SIDESPIN`, `PINLAB_DAMPER`; `tools/dev/ramps/make_ramps.py` drives it) and `gutter` (a ball put above a gutter). Configurations are named in `Named()`: `stock` (the game), `bp`, `ccd`, `spec`, `dt375`, `dt25`, `co5`, `co10`, `it20`, `ref` (converged), and pin friction variants: `stock_pfNN` / `dt375_pfNN` (e.g. `stock_pf04` = the pin material's friction 0.40).
- Materials use each one's own combine mode from `scene.txt` (the game's are Multiply; 1.7.0-1.7.4 wrongly used Minimum).
- `scene.txt`: the scene exported from the game's files (1.907). `export_scene.py` re-exports it (UnityPy).
