# Developer checks (no device needed)

| File | What it does |
|---|---|
| `host_check.sh` | Compiles and links all Android native code on the host with stub headers (`stubs/`). Only `JNI_OnLoad` may be exported. |
| `ios_order_scan.py` | Finds static functions used before they're declared in the iOS sources (what broke the 1.6.1 iOS build). `Backup.mm crc32` is a known false positive. |
| `pinsim/run.py` | Runs the live pin-turn block from `android/native/Game.cpp` against a fake lane that racks exactly like the game's decoded `RunPsycsTest.UpdatePinPositions` (one `Random.Range(0, 16)` per kegel, through the engine-function pointer) and a fake pinsetter with the real model rotations and joint poses from the game's scene and clips: 33 checks (pickups, gutter, spare, strike, open frame, back-to-back racks, spare-mode racks, online, failed reads, and what the pinsetter shows). |
| `pinhook/check.py` | Checks the pin-turn mechanism against the real game binaries: runs the game's own `Random.Range` code in an ARM64 emulator (`pip install unicorn`) and runs the shipped decoders (`RandSlotIn`, `FindTurnSites`) on both binaries mapped at their own layout. `--so libil2cpp.so --ios UnityFramework`. Addresses are for 1.907; see the script for updating them. |
| `android_offline_build.sh` | Builds `libmain.so`, `bowlingplus.dex` and (given the game's `.apks`) the signed APK when Google's and Maven's servers are blocked: toolchain pieces from GitHub and the Ubuntu archive. Output is byte-identical to the manual build used for 1.6.6. |
| `il2cpp_meta.py` | Reads the game's `global-metadata.dat` (v31: types 88 bytes, methods 36, fields 12): class, field and method names. |
| `dex_classes.py` | Reads the game's `classes*.dex`: which classes/methods/fields exist (to check Java reflection targets). |

The iOS code compiles on Linux too: Theos from GitHub, L1ghtmann's `iOSToolchain-x86_64` release and `iPhoneOS16.5.sdk`
from `theos/sdks` (see `VERIFIED_NOTES.md` section 10). Shared game logic still goes into `android/native/Game.cpp`
and `src/Game.mm` as byte-identical blocks (per-platform bits through macros just above a block, e.g.
`PIN_TURN_CLOCK` / `PIN_TURN_LOG`).

For reading the game's code: Il2CppDumper's net6 release runs on Ubuntu's `dotnet8` with `DOTNET_ROLL_FORWARD=Major`;
disassemble with `pip install capstone`. On Android, metadata pointers in code resolve through the `.so`'s RELA
relocations (the file holds zeros there).
