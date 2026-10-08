# Engine fixed step: how the double physics rate works

From `VERIFIED_NOTES.md`, section 5g. Included so the double-rate change can be checked against the game binaries (`tools/dev/physrate/check.py`).

## 5g. The engine's fixed step [files, 1.7.1]

- `Time.set_fixedDeltaTime` is stripped from the scripts **and** from the engine's internal-call table; `Time.get_fixedDeltaTime` survives natively (icall name `UnityEngine.Time::get_fixedDeltaTime`, no "()").
- **Android `libunity.so`**: registered at `0x4b0ff0` (adrp/add of the name, `adr x1` of the function, branch to the registrar), native getter `0x4ad2d8`: `bl 0x5c2ad8` (the TimeManager getter), then `ldr x8,[x0,#0x50]`, `ldr w9,[x0,#0x58]`, `ldr s0,[x0,#0x5c]`: step = count (int64 +0x50) x den (u32 +0x5c) / num (u32 +0x58). Stored as 1058399 / 141120000 (Unity's 141,120,000 ticks per second) = 7.5 ms.
- **iOS `UnityFramework`**: registered at `0xb7283c`, native getter `0xb6e828`: `bl 0xc7de34`, then the same three loads.
- **The TimeManager's sync** (a virtual method on both: Android `0x5c2568`, iOS `0xc7d828`): `+0x84` = (step > epsilon ? 1/step : 1) as a float, then copies 16 bytes `+0x50` -> `+0x70`. A load-time routine (Android `0x5c1a90`, iOS around `0xc7cddc`) also copies `+0x50` -> `+0x60` and stores `+0x84` the same way. The consistency check (Android `0x5c25a8`) clamps the step between a minimum and 10 s and raises floats at `+0x1b0` and `+0x1b4` to at least the step (no change for a smaller step).
- `Time.set_timeScale` (native `0x4ad318` -> `0x5c271c`) only stores the scale at `+0x1ac`; it doesn't resync the step.
- **1.7.1 writes** the halved count to `+0x50` and `+0x70` and 1/new step to `+0x84`, after checking `+0x70` equals `+0x50` (count and rate), `+0x84` is 1/step within 1 %, and the managed getter returns the same step; then reads the getter back. Found at runtime through `il2cpp_resolve_icall` and the getter's first BL (checked to be followed by the `+0x50`/`+0x58` loads). `tools/dev/physrate/check.py` checks this against both binaries.
- **Per-step game code** with the double rate: the oil transfer walks the cells between the previous and current contact (`redistributeOil`), so it follows the distance travelled, not the number of steps; the lane friction is rewritten every step (not accumulated). Not checked on a device.

