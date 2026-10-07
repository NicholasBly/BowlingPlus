# pinlab

The game's pin physics rebuilt on NVIDIA PhysX 4.1 (the engine Unity 6 embeds), with the game's own pins, materials,
lane, rack and physics settings. Used for `PIN_PHYSICS_STUDY.md`.

- `build_physx.sh`: builds PhysX 4.1 from GitHub (static, Linux, clang) and `pinlab`.
- `pinlab.cpp`: the harness. Experiments: `rest`, `ballpin`, `pinpin`, `rack` (`PINLAB_N` throws), `bowlscore` (USBC's 23 offsets x 11 angles; `PINLAB_SHOTS`, `PINLAB_V`). Configurations are named in `Named()`: `stock` (the game), `bp`, `ccd`, `spec`, `dt375`, `dt25`, `co5`, `co10`, `it20`, friction variants, `ref` (converged).
- `scene.txt`: the scene exported from the game's files (1.907). `export_scene.py` re-exports it (UnityPy).
