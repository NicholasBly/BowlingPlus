# Bowling by Jason Belmonte: verified notes for future tweak work

These are the facts about the app that were actually checked while building BowlingPlus (v1.0.0 to v1.6.7). The project was called BowlingFix until 1.4.0. Anything not checked is listed separately at the end under "Not verified yet". Names, offsets and addresses are for **app version 1.907 (build 1597)** only. A game update can change any of them, so re-check with a fresh dump after updates.

How each fact was checked:

- **[files]**: read from the game's own files in the IPA (Il2CppDumper dump, ARM64 disassembly of `UnityFramework`, Unity asset files, bundle manifests, `Info.plist`).
- **[device]**: seen on the user's iPhone (screenshots, BowlingPlus's "Copy debug info" output, a crash log).
- **[web]**: public product info.

## 1. The app

| Fact | Value | Source |
|---|---|---|
| Bundle id | `studio.wannaplay.bowlingjb` | [files] Info.plist |
| Version | 1.907 (build 1597) | [files] Info.plist |
| Executable | `BowlingbyJasonBelmonte` (game code lives in `Frameworks/UnityFramework.framework/UnityFramework`) | [files] |
| Minimum iOS | 13.0 (`DTPlatformVersion` 26.2) | [files] Info.plist |
| Engine | Unity `6000.0.67f1` (`6000.0.67f1_78a1c2bbeb6a`), IL2CPP, arm64 | [files] UnityFramework strings |
| Publisher SDK | MRGS (My.Games). A fresh install shows its Korean privacy consent screen (약관 및 개인정보 취급방침) | [device] |
| Test device | iPhone18,1 (iPhone 17 Pro), iOS 27.0.1, not jailbroken | [device] debug info |
| Install method | Signulous ("install as duplicate app" gives a new bundle id and its own separate save data) and Sideloadly | [device] |

Game data layout [files]:

- `Data/sharedassets0.assets` holds the **ball skin catalog** (`ItemsVisualData`, see section 6), stored uncompressed.
- `Data/Raw/AssetBundles/` holds the asset bundles: `balls/fulltextures`, `balls/icons`, `balls/cores`, `pins/*`, `shoes/*`, `gloves/*`, `data`, `prefabs`, `img`, `packs`, `wallart`, plus `ResourceMeta.mdt`. Each bundle has a plain-text `.manifest` that lists every asset path inside it (1174 asset files in total). This is the easiest way to check whether a texture ships with the app.
- `Data/globalgamemanagers` holds the project settings (physics and time, section 2).

Third-party SDKs [files: `Frameworks/`, the IL2CPP assembly list, and Objective-C class names in `UnityFramework`]:

| Kind | SDKs |
|---|---|
| Ads | AppLovin MAX (+ AppLovinQualityService), Google Mobile Ads (AdMob), ironSource, Unity Ads, Meta Audience Network. Info.plist lists 156 `SKAdNetworkItems` |
| Analytics / attribution | Yandex AppMetrica ("YandexMobileMetrica" lines in the console [device]), AppsFlyer (+ PurchaseConnector), Facebook App Events (`FacebookAutoLogAppEventsEnabled` and `FacebookAdvertiserIDCollectionEnabled` are both YES), Firebase Crashlytics + Messaging, Unity Analytics + Unity Services telemetry, My.Games MRGS (`games.my.mrgs.core` / `.gdpr`), the game's own `ClientAnalitics` (host `clickhouse.wannaplay.studio`) |
| Accounts, purchases, other | Facebook Login/Share/Gaming Services, Sign in with Apple (`AppleAuth`), Unity IAP, Photon (`Photon3Unity3D`, online play), Unity iOS notifications, Prime31 `P31RestKit` |

Network [files]:

- Info.plist sets `NSAllowsArbitraryLoads = YES`, so plain HTTP is allowed.
- The game's own API base string is `http://api.wannaplay.studio/scripts`, which is plain HTTP. Replay sharing uses `http://replays.spareball.com/replay_shared/*.php`.
- No `CertificateHandler` subclass or certificate-validation callback exists in `Assembly-CSharp`, so there is no TLS-check bypass.

Info.plist oddities [files]:

- `NSCameraUsageDescription` says "photo library" and `NSLocationWhenInUseUsageDescription` says "camera". The two texts are swapped.
- `CADisableMinimumFrameDurationOnPhone = NO`, so iOS caps the app at 60 Hz.

## 2. Physics and time settings [files]

Read from `globalgamemanagers` (PhysicsManager and TimeManager) with UnityPy.

| Setting | Value | Note |
|---|---|---|
| Gravity | (0, 0, -9.81) | **Z is up and down.** Down the lane is +Y (see PinPhysic below) |
| Fixed timestep | 0.0075 s (133.3 Hz) | stored as rational 1058399 / 141120000 |
| Maximum allowed timestep | 0.1 s | |
| Default contact offset | 0.0005 | Unity's default is 0.01 (20x bigger) |
| Solver iterations / velocity iterations | 7 / 7 | |
| Default max angular speed | 7 rad/s | old Unity default; newer projects use 50 |
| Bounce threshold | 0.03 | |
| Sleep threshold | 0.01 | |
| Default max depenetration velocity | 10 | |
| Solver type / friction type | 0 / 0 | PGS solver |
| Enhanced determinism / auto sync transforms | off / on | |

## 3. Runtime rules learned the hard way

- **GC handles are 64-bit.** `il2cpp_gchandle_get_target` (export at `0xaaa08c`, real code at `0xaae100`) starts with `and x22, x0, #0xffffffffffffe000`. The handle encodes an address, so storing it in a `uint32_t` corrupts it. BowlingPlus v1.0.0 did that and the game froze on the loading screen until iOS killed it (watchdog `0x8BADF00D`). [files] disassembly + [device] crash log
- **Some getters create objects.** `Managers.ItemHolder.get_Instance()` (RVA `0x1E4A5A8`) calls `ItemHolder..ctor` when there's no instance yet. Read the static `ItemHolder._instance` instead. BowlingPlus also reads the static `InventoryHelper._currentBall` instead of calling `get_CurrentBall()`. [files]
- **Waiting before touching anything works.** Since v1.0.1, BowlingPlus does nothing for 5 seconds after launch, then waits until the lane controller (`RunPsycsTest`) has existed for 4 seconds. No more freezes since. [device]
- **No hooking on a non-jailbroken phone.** BowlingPlus polls once per frame (CADisplayLink) and calls game code through `il2cpp_runtime_invoke`, which also catches C# exceptions. Static fields are read with `il2cpp_field_static_get_value`. All of this works on device. [device]
- **Stripped Unity APIs** (left out of the build, so they can't be called) [files, dump]:
  - `Collider`: only `get_material` survives. **No `contactOffset` setter.**
  - `Rigidbody`: `get/set_linearDamping`, `get/set_angularDamping`, `set_maxDepenetrationVelocity`, `get/set_isKinematic`, and `set_maxAngularVelocity` survive (the angular velocity getter doesn't). Per-body solver iteration setters are gone.
  - `Physics`: none of the default-value setters (contact offset, solver iterations, and so on).
  - `PhysicsMaterial`: `get/set_dynamicFriction`, `get/set_staticFriction`, `get_bounciness`, `get_frictionCombine`.
  - `Time`: `get_fixedDeltaTime` exists, **`set_fixedDeltaTime` doesn't**.

## 4. Game state

- `GameParams` (static class) [files]:
  - `gameMode` (`GameModes`, static, offset `0x3C`): `FUN`=0, `COMPETE`=2, `NONE`=4, `TUTORIAL`=5. Practice runs as `FUN` [device: `mode=0` in Practice].
  - `_playerLocation` (`PLAYER_STATE`, static, offset `0x2C`): `UPPER_SCREEN`=0, `BALL_RETURNER`=1, `BUTTOM_MONITOR`=2 (sic), `START_POS`=3, `THROWING`=4, `ON_PINDECK`=5, `REPLAYER`=6. `loc=3` while aiming [device].
- `RunPsycsTest` is the main lane and throw controller (a MonoBehaviour). Useful fields:
  - `sphere` (`0x80`): the physics ball GameObject.
  - `sphere_render` (`0x88`): the ball you see in your hand.
  - `_kegsUp` (`bool[]`, `0x68`): standing pins, where slot i is pin i+1.
  - `kegsUpdate` (`Action<bool[]>`, `0x28`): the event the game fires after racking.
  - `UpdatePinPositions()` re-racks from `_kegsUp`. Writing `_kegsUp`, calling it, then firing `kegsUpdate` gives a custom rack. BowlingPlus's spare mode works this way [device].
- **Frame rate** [files]: the game takes its frame rate from static ints in `Client.Core.Constants`:
  - `FB_FRAME_RATE` (`0x44`) = **200**, used on PC (the old Facebook GameRoom build).
  - `MENU_FRAME_RATE` (`0x48`) = **30**.
  - `GAME_FRAME_RATE` (`0x4C`) = **60**.
  - The values come from a 16-byte constant at VA `0x3e9f8d0`, copied in by the `.cctor`. The 4th value is `MAX_FRAME_SCORE` = 30.
  - **[files]** The stock IPA's `Info.plist` has `CADisableMinimumFrameDurationOnPhone` = **false**, so iOS holds the game to 60 Hz whatever `targetFrameRate` says. `tools/inject_ipa.py` sets it to true. **[device, 1.6.1]** An install made without it (`plist120=0`) ran 120 FPS mode at 60 Hz even though the game reported `target=120`; 1.6.0 was reported smooth (how that copy was installed wasn't recorded).
  - `GetFPSForGameMode()` (RVA `0x1D89A10`) returns `GAME_FRAME_RATE` when `Application.platform` is iPhone (8) or Android (11), and the PC value otherwise. Menus work the same way through `GetFPSForMenuMode()`.
  - Callers that pass these values to `Application.set_targetFrameRate`: `Loading.Start`, `RunPsycsTest.StartFun`, `RunPsycsTest.restartGameComplite`, `FloatingMenu`, `HTHLobbyManager`. They all set `QualitySettings.vSyncCount = 0`, which iOS ignores anyway.
  - `get/set_targetFrameRate` both survive stripping, and Unity's iOS layer uses `setPreferredFrameRateRange:`.
  - BowlingPlus 1.2.0's 120 FPS mode writes 120 into both table values (`il2cpp_field_static_set_value`), and the patched IPA sets `CADisableMinimumFrameDurationOnPhone = YES`.
- **Ball speed:** the ball is launched with one push. Multiplying the ball Rigidbody's `linearVelocity` once, right after the launch (`THROWING`, speed > 1 m/s), works [device]. Above about 5x the user found it too much. `InventaryData.ResetRigidBody` (RVA `0x1DC2040`) is the only place the game sets damping, max angular velocity and max depenetration velocity, and it does so for the **ball only** [files].

## 5. Pins

- Path to each pin's Rigidbody: `PinHolder._pins` (`List<Pin>`, `0x20`) → `Pin._physic` (`PinPhysic`, `0x40`) → `PinPhysic._rigidbody` (`0x38`). [files]
- `PinPhysic` [files, disassembly]:
  - `Awake` sets a custom `centerOfMass` (`_centerOfMass`, `0x40`) and `inertiaTensor` (`_inertiaTensor`, `0x4C`), with `inertiaTensorRotation` = identity.
  - `FixedUpdate` adds a force only when `transform.position.y > 19.2` (the pit area). That's how we know +Y is down the lane.
  - `OnCollisionEnter` only invokes the `OnCollision` action (`0x58`).
  - There are no kinematic or damping tricks on standing pins.
- The game never changes pin damping or max angular velocity. Pins use the project defaults: Discrete collision and a max spin speed of 7 rad/s [files: the only callers of those setters are in `ResetRigidBody`]. On device, pin `linearDamping` = 0 and `angularDamping` = 0 [device].
- **Pin prefab** [files: `Raw/AssetBundles/prefabs`, `PinRuntimeObject.prefab`]:
  - **Rigidbody:** mass **1.644**, drag 0, angular drag 0, no interpolation, Discrete collision.
  - **Colliders:** five convex MeshColliders (`pin_top001`, `pin_middle_1`, `pin_middle_2`, `pin_bottom1`, `pin_bottom`), two tiny CapsuleColliders (`Capsule001`, `ball001`), and a BoxCollider `Zone`.
  - **Physics materials:** "Pin" has dynamic friction 0.5, static friction 0.3, bounciness **0.65**, both combine modes = 1 (Minimum). "PinButtom" (on `pin_bottom`) is the same but with bounciness 0.4.
- **PinPhysic values come from code, not the prefab.** Its fields are private, and the prefab's serialized data is only 36 bytes. The constructor sets:
  - `_centerOfMass` = (0, **0.1498**, 0), about 15 cm up the pin's long axis, roughly where a real pin's balance point is.
  - `_inertiaTensor` = (**0.013934, 0.001915, 0.013934**): tipping, spin, tipping. These are realistic, so the mass properties aren't the problem.
  - `_forceDir` = `Vector3.up`.
- **Why some pin hits are weak** (analysis, not proven on device):
  - The contact offset is 0.0005 (its setter is stripped), and a physics step is 7.5 ms.
  - A tumbling pin's top can move about 5 m/s from spin alone, so it ends up deep inside another pin before the contact is found. The solver then pushes them apart instead of transferring the hit.
  - Sweep CCD (`ContinuousDynamic`) only follows straight-line motion; `ContinuousSpeculative` (enum value 3, present in this build) also predicts rotation.
  - **Never put the ball on `ContinuousSpeculative`** [device, BowlingPlus 1.2.0]: the ball jumped into the air right before the pins and sailed over them. The ball spins at ~600 rpm, so speculative contacts reach far ahead and create "ghost" contacts with the pin deck and pins.
  - Since 1.2.1 the ball always uses `ContinuousDynamic`. Speculative is an experimental, pins-only option that is off by default.
- Switching pins (and the ball) to ContinuousDynamic collision (`collisionDetectionMode` = 2) works [device: `cdm=2` in debug info]. The user said the pins "feel much better" after this [device].

## 5a. Pin turns: what the game does when it racks [files, 1.6.6]

Read from the code itself (Il2CppDumper 6.7.46 + capstone, Android `libil2cpp.so` and iOS `UnityFramework`, 1.907). Both builds do the same thing; addresses below are Android RVA / iOS RVA.

- **The pins on the lane are `InventaryData.kegels`** (`GameObject[]`, `0x88`; `UsePinHolder` `0x158` is false). Each kegel GameObject carries the Rigidbody. `kegels_renderers` (`0x90`) are the visible roots.
- **`RunPsycsTest.UpdatePinPositions`** (`0x1791964` / `0x1EBCFD4`), kegel path:
  1. Copies `_kegsUp` into the static `mdl_ShootCurrentData.CurrentData` (`mdl_ShootData`) field `Before` (`bool[]`, `0x20`). Other `mdl_ShootData` fields: `ballId 0x10, throughid 0x14, speed 0x18, RpM 0x1C, Previos 0x28, Current 0x30, States 0x38`.
  2. For each kegel `i`, in order: `KegelSoundManager.enabledSound` (`0x60`) = true; position = `Client.Core.Constants` static `Vector3[]` (`0x198`)`[i]`, with y + `Constants` static float `0xB8`, z = **-0.0005** when `_kegsUp[i]` (standing), else z = **-200** (under the floor); then **`transform.rotation = Euler(0, 0, Random.Range(0, 16) x 22.5)`**: a random turn about world **Z** (world up; gravity is -Z). Then `InventaryData.ResetRigidBody(kegel)`, Rigidbody `centerOfMass` = RunPsycsTest `0x1DC`, `inertiaTensor` = `0x1E8`, `inertiaTensorRotation` = identity, `Sleep()`; `kegels_renderers[i]` gets the same position and **identity** rotation.
  3. Every kegel gets a `Random.Range` call, up or down: exactly one call per kegel, in order, per rack.
  4. With `UsePinHolder` on it calls `PinHolder.SetupPins(_kegsUp)` instead (not used on this build).
- **Who racks:** `UpdatePinPositions` <- `ChangeBall` (`0x179775C`; <- `toSmallScreen`, `toSmallScreenMonitor`, `changeBall`, `delegateUnhadnled` x2: picking up / switching a ball), `Reset` (`0x1794038`; <- `endThrought`, `endThroughtByTimeout`, `throwBallPracticeComplete`, `StartFun`, `ChooseBall`, `Update_3`, replays, tutorial stages, `SkipTutorial`, `InfoScreenManager.SetupRemoteSession`), `updateAfterLoadSession` x2, `PlayDemoReplay`.
- **`ChangeBall` and `Reset` call `RecalculatePinsUpState` (`0x1796A74`) first:** it re-activates every `kegels_renderers`, and in modes FUN / COMPETE / TUTORIAL: if `InfoScreenManager.curAttemptsCount == 2` all of `_kegsUp` = true, else all false and then true for each 1-based pin number in RunPsycsTest's `int[]` at `0x160`; then it fires `kegsUpdate`. So a pickup re-racks with the same pins up, and a new frame racks all 10 up **directly** (there is no in-between empty rack after a strike).
- **`InventaryData.PlaceKegs`** (`0x16B6D64`, only from `ArsenalPinManager.managePins`) uses `Quaternion.identity`. Other `Random.Range` users near pins: `tutorialBotTurn`, `PinHolder.SetupPins`, `SoundDescriptionData.GetRandomPinRoll`.
- **How `Random.Range(int, int)` reaches the engine** (`0x2D4FC98` / `0x384DBDC`; `RandomRangeInt` `0x2D4FCDC` / `0x384DC40` is identical): `adrp xN, page; ldr x2, [xN, #off]`; if null, resolve `"UnityEngine.Random::RandomRangeInt(System.Int32,System.Int32)"` and store it; restore the saved registers **including x30**; `br x2`. The pointer lives in writable data: Android `.bss` `0x33DE700`, iOS `__DATA` `0x4E39030`. It's a tail call, so the engine function is entered with the **caller's** return address: `UpdatePinPositions`' call (`bl` at `0x1791D30` / `0x1EBD38C`, with `mov w1, #16` just before) returns to `0x1791D34` / `0x1EBD390`. Checked by running both binaries' own wrapper code in an ARM64 emulator (unicorn): entered with that return address, w0 = 0, w1 = 16, stack and frame pointer restored. `il2cpp_resolve_icall` is exported by both. UnityFramework is plain arm64 (no pointer authentication); BowlingPlus's `libmain.so` has no BTI property note.
- **1.6.6 uses exactly that** (`BFRandomRangeInt` in `Game.mm` / `Game.cpp`): it points the slot at its own function (a data write), which calls the engine first (the game's random sequence is unchanged) and, only for that return address with (0, 16), returns the kegel's kept turn (from 1.6.7, a fresh turn is one decided in advance so the pinsetter can show it, section 5b). The slot and the call site are found at startup by decoding the code (`RandSlotIn`, `FindTurnSites`), which `tools/dev/pinhook/` checks against the real binaries.
- **Why 1.6.2-1.6.5 couldn't work:** they let the game turn the pins and then turned them back from a poller, so a new turn could always show for at least one frame, and their pose code split the turn about world **Y** although the game turns pins about **Z**.

## 5b. The pinsetter's pins [files, 1.6.7]

- **[device, 1.6.6]** Pins no longer turned on ball pickups. During the pinsetter's lift for the second ball, every lifted pin showed the same face (the back of a custom pin image); once racked for the second ball they had their own turns again.

Read from the code, `level1` and the pinsetter's animation clips (UnityPy; the scene's MonoBehaviour type trees are stripped, rebuilt from Il2CppDumper's DummyDlls with `TypeTreeGeneratorAPI`).

- **The pinsetter never carries the real pins.** `PinSetterManager.runPinsetter(fs, up, up_)` (<- `RunPsycsTest.endThrought`, `endPlayingThrought`) plays the clips and stores the standing-pin numbers (1-based) in `upKegs` (`0x40`) / `upKegs_` (`0x48`). `PinSetterManager.Update`: when the first part has finished (`firstPartAnimation` `0x38`, animation not playing) it `SetActiveRecursively(true)` on `InventaryData.pinseterKegelRenderer[upKegs[j] - 1]` (`GameObject[]`, `0xC8`) and `SetActive(false)` on `kegels_renderers[upKegs_[j] - 1]` (the real pins' visible models), then plays the next clip. Every frame it copies each `pinseterKegelRenderer[i]` root's world position and rotation onto `pindeckKegelRenderer[i]` (`0xD0`) and gives it the same active state. `updateMirroredKegs` gives `mirrorOnPinsetter[i]` (`0x50`) the active state of `pinseterKegelRenderer[i]`. `InventaryData.DisablePinsetterPinRenderers` is called from `RunPsycsTest.Reset`.
- **The real pins' visible models** (`kegels_renderers[i]`, `PinsRender/PinsRender/candlepin_graphicsN`) get the kegel body's world position and rotation every frame (`RunPsycsTest.Update_4`, when `UsePinHolder` is off). Their mesh children (`pin_graphics`, `pin_graphics_rep` = `kegels_renderers_h`, mesh `polySurface060`) sit at local `Rx(90)` (`(0.7071, 0, 0, 0.7071)`); the mesh stands along its own Y.
- **The pinsetter's pin models:** `PinSpotter/animation_mashina/pinspotter_1:low_grp_export/.../pinspotter_1:temp:joint17/pinspotter_1:jointNN/candlepin_graphicsN` (N = pin number), root local `(0.9997595, 0.0143005, -0.0166272, -0.0002378)` (about `Rx(180)`), mesh child `pin_graphics` at local `Rx(90)`. In the scene's rest pose they lie flat. The legacy `Animation` on `animation_mashina` has `grabliDown, goDown, goUp, goDownTakePins, goDownPlaceKegs, goUpSecond` (uncompressed rotation curves); **only the joints are animated (`pinspotter_1:temp:joint17` and each `pinspotter_1:jointNN`), never the model roots**, so a rotation set on a root stays.
- **At the pick-up (end of `goDownTakePins`) and the placing pose (end of `goDownPlaceKegs`) every root stands upright (tilt 0.27 degrees) turned -3.544 degrees about world Z**, the same for all 10 pins (joint world rotation there: `(0.9997596, -0.0166267, -0.0143009, 0.0002377)`). So twisting a root's local rotation by `Rz(turn + 3.544)` (its local Z is the pin's long axis, pointing up there) makes the model face exactly like a real pin with that turn (checked numerically: under 0.001 degrees).
- **Reflections:** `mirrorOnPinsetter[i]` = `PinSpotter/animation_mashina_mirror/.../mirror_render_N` under a second copy of the machine whose root is `(0, 0.7071, -0.7071, 0)` (upside down, **no negative scale**), root local = the same rotation (negated quaternion), pin mesh `keg` at local `Rx(90)`. Its long axis points down at the pick-up, so it needs `Rz(3.544 - turn)` to face like the floor reflection of the real pin. (The real pins' reflections, `MirrorRender/mirror_render_N`, use scale z = -1 instead.)
- **1.6.7 uses this:** `PinsetterTurnTick` (in the shared pin block) sets each root, only while it's hidden (`activeInHierarchy` false), to its own rotation times that twist, using the turn the pin has or will get at the next rack (`TurnPredict`: same rule as the rack itself; a pin that fell gets its next turn decided in advance, `sTurnPend`).

## 6. Ball skins (the DLC system)

### How a ball gets its 3D skin [files]

1. Each store entry is a `Models.mdl_Shop_Item`:
   - `itm_id` (`0x10`), `itm_name` (`0x18`), `i_type` (`0x40`), `dlc_link` (`List<int>`, `0x50`).
   - Balls are `Models.mdl_Shop_Item_Ball`, which adds `texc` (`bool`, `0xE4`).
2. `getDLC(DLC_TYPE type, DLC_SUB_TYPE sub)` (RVA `0x1CFD3A0`) walks `dlc_link`. It looks each id up with `ItemsVisualData.GetItemDataByID` and returns the **first** id whose record has the same `dlc_type` and `dlc_sub_type`, or **-1**. 3D skins use `DLC_TEXTURE` (3) with sub-type 0.
3. **The catalog** is `ItemsVisualData`, a ScriptableObject in `sharedassets0.assets` that the game reaches through `Director.Instance.DataObject` field `0x20`:
   - `_itemDatas` (`List<ItemsVisualData.ItemData>`, `0x18`).
   - `ItemData` is a **struct** `{ mdl_DLS_Item Item @0x0; int ID @0x8 }`.
   - `Models.mdl_DLS_Item` has `dlc_id` `0x10`, `dlc_type` `0x14`, `dlc_sub_type` `0x18`, `dlc_url` `0x20`, `dlc_version` `0x28`, `dlc_create_date` `0x30`.
4. 1.907 ships **982 catalog records**: 388 TEXTURE (type 3), 588 ICON (type 2), 6 Ad (type 0). In the file, each record is stored as `dlc_id, type, sub, url (int32 length + ASCII, padded to 4 bytes), version, date, ID`, and `ID` equals `dlc_id`.
5. `dlc_url` is **an asset path, not a web address**, for example `assets/bundledata/art/balls/fulltextures/text_matchuppearl_matchuphybrid.png`. `DLCClientManager.loadItemWWW` (RVA `0x1CAFE14`) checks `Client.ResourceSystem.ResourceSystem.IsAssetExist(url)`, then calls `LoadAssetAsync`. When the asset is missing it logs "asset doesn't exist with: id: {1}; cashe: {2}; url: {0}".
6. `Managers.DLCManager.all_server_dlc` (static `Dictionary<int, mdl_DLS_Item>`) is **not** the live catalog. On device, looking up every ball's skin id in it failed [device, v1.1.1]. Use `ItemsVisualData.GetItemDataByID` (RVA `0x1CAFAC8`) instead.
7. Enums: `DLC_TYPE` is Ad 0, Replay 1, ICON 2, TEXTURE 3, ASSET 4, LOCAL_XML 5, CSV 6. `DLC_SUB_TYPE` is NONE 0, then TYPE_1 to TYPE_6.

### The loader on each 3D ball [files, disassembly]

`LoadDLCContentTexture` (MonoBehaviour) has fields `toChangeTexture` (`GameObject[]`, `0x30`), `offset` (`Vector2`, `0x38`), `dlcIDToLoad` (`0x40`) and `objectID` (`0x44`).

- `loadTextureForItem(item, subType, switchID)` (RVA `0x1CC7920`):
  1. Stores `switchID` in `objectID`.
  2. Calls `getDLC(TEXTURE, subType)`.
  3. If that returns **-1, it returns and nothing changes.** The ball keeps the texture it already had.
  4. Otherwise it stores the id in `dlcIDToLoad` and calls `DLCMemoryCash.getDLCForItemId(id)`.
- `textureLoaded(tex)` (RVA `0x1CC7684`) runs for each renderer in `toChangeTexture`:
  1. Sets `material.mainTexture = tex`.
  2. Sets the half of the sheet: if `objectID == 1`, `mainTextureOffset = (0,0)`; if it's 0, `mainTextureOffset` = the component's own `offset`.
  3. Finally sets `dlcIDToLoad = -1`.
- Callers: `InventaryData.PrepareBallRenderer` passes the shop ball's `texc` as `switchID`. `InitBallsOnReturner`, `ActivateAllOnReturner`, `RollOutCurrentBallOnReturner` and `ReplayerBallinfoManager.manageBallInfo` call it too. The other loader, `LoadDLCContentTextureMaterial`, is only used for **pins** (`InventaryData.PreparePinRenderer`).
- So when a skin file is missing, the 3D ball shows **plain white** (a fresh renderer) or **the last ball's skin** (a reused renderer). [device: the user saw exactly that for the Match Up BP]

### Two-ball texture sheets

Most ball texture files hold two balls side by side and are named after both. The **first-named ball is the left half and uses `texc` = 0**. The **second-named ball is the right half and uses `texc` = 1**. [device] This was checked on `text_matchuppearl_matchuphybrid.png`: the Pearl with texc 0 shows burgundy, the Hybrid with texc 1 shows blue/green/yellow, and the user confirmed both look right. The Solid has texc 0 on `text_matchupsolid_mixblackout.png`, which is consistent.

### The Match Up skin bug (root cause)

| Ball | Skin id the game uses | File in the catalog | Ships in the app? | Result |
|---|---|---|---|---|
| Match Up Pearl | 770, texc 0 | `tex_blackpearl_flameturquoise.png` | yes | wears the **Black Pearl's** art (wrong ball) |
| Match Up Hybrid | 381, texc 1 | `text_matchuppearl_matchuphybrid.png` | yes | correct |
| Match Up BP | 413, texc 1 | `black_pearl2_dif_x512.jpg` (dated 15.06.2018) | **no**, it's in no bundle manifest | never loads: white, or the last ball's skin |
| Match Up Solid | 385, texc 0 | `text_matchupsolid_mixblackout.png` | yes | not reported broken |

Ids and texc come from [device] debug info. Files and the shipped check come from [files]: the catalog plus every `.manifest`. Every other Match Up skin file ships; 413 is the only missing one.

Other Match Up catalog records [files]:

- Textures: 196 `match_pearl_solid.png`, 506 and 517 `tex_tropicalsurgetb_matchupbbr.png`.
- Icons: 382 `obj_ball_matchup_hybrid`, 383 `obj_ball_matchup_pearl`, 386 `obj_ball_matchup_solid`, 446 `obj_ball_matchup_black`, 516 `obj_ball_matchup_bbr`.

The game's Pearl icon (burgundy/black/silver) matches the left half of 381 [files]. The user confirmed burgundy is right for the Pearl [device]. Storm lists the real Match Up Pearl as black/orange/silver [web: bowling.com, BowlingThisMonth].

### Fix that works on device (Pearl, Hybrid and BP)

1. Find the store entries in `Managers.ItemHolder._instance` (static) → `_shop` (`ShopItemHolder`, `0x20`) → `_shopItems` (`List<mdl_Shop_Item>`, `0x10`), matching on `itm_name`.
2. Edit them in memory: call `List<int>.Remove` / `Add` on `dlc_link` until `getDLC(3,0)` returns the right id, then set `texc`.
3. Re-rack with `InventaryData._instance.InitBallsOnReturner(true)` (RVA `0x1DC317C`). The game itself calls this when your inventory changes. BowlingPlus only calls it while `loc` is 0 or 1.
4. Reload the ball in your hand with `LoadDLCContentTexture.loadTextureForItem(item, 0, texc)` on the component under `sphere_render`.

[device: the user confirmed the Pearl and Hybrid are fully fixed with Pearl → 381, texc 0, and the BP is fixed with BP → 770, texc 0 (BowlingPlus 1.1.2)]

To find the current ball, read `Managers.InventoryHelper._currentBall` (static `mdl_Item_Ball`). Its `mdl_Item.item_id` (`0x14`) or `base_ide` (`0x10`) is passed to `ItemHolder.GetShopItemById` (RVA `0x1E4E040`).

## 7. Tutorial [files + device]

- `Bowling.GUI.TutorialManager` has a static `Instance` (`0x8`) and `_activeStage` (`0x60`).
- `IsInTutorial()` (RVA `0x1E46E94`) returns `_activeStage != null && <virtual check on that stage>`.
- `SkipTutorial()` (RVA `0x1E484A8`):
  - logs `TUTORIAL_SKIP`,
  - calls `ClientSideManager.set_IsTutorialCompleted(true)`,
  - calls `RunPsycsTest.RestartAfterTutorial`, `Reset`, `toTopScreenFromAny` and `switchToFun`,
  - calls `InfoScreenManager.switchOnePlayer`.
- **It never clears `_activeStage`, so `IsInTutorial()` keeps returning true after a skip.** [device: BowlingPlus 1.1.0's button stayed on screen]
- `ClientSideManager.get_IsTutorialCompleted()` (static, RVA `0x1CAC348`) reads `CrossPrefs.GetInt` or `PlayerManager.tutorialComplete`. This is the reliable "done" flag.
- Calling `SkipTutorial()` from a tweak works and lands on the main menu [device]. A button driven by the "tutorial completed" flag hides correctly after a skip [device, 1.1.1].
- **Kick-back after a skip** [files + device]: the leftover `_activeStage` keeps receiving events.
  - On the first throw, `HanldeThrowEnd` (RVA `0x1E47E00`) runs the stage and `ApplyState`. The stage ends, and `HandleTutorialStageEnded` (RVA `0x1E489D4`) does three things: unhooks `BaseTutorialStage.OnStageEnded` (`0x10`), sets `_activeStage = null`, and calls `closeTutorial()`.
  - `closeTutorial()` calls `RunPsycsTest.RestartAfterTutorial` and `switchToFun`, which is the jump back to the main menu. The user saw it once after skipping and logging in.
  - Every handler that reads `_activeStage` null-checks it first (`HanldeThrowEnd`, `HandleFirstKegCollision`, `DragManager_dragchange`, and all three reads in `ApplyState`). `HandleGameEnded`, `HandleLevelUp` and `HandleEndSession` are empty.
  - So clearing `OnStageEnded` and `_activeStage` right after `SkipTutorial()` is safe. BowlingPlus 1.2.0 does that.

## 7a. Startup and the loading screen [files]

- **Startup state machine:** `Client.CoreLoop.MonoCoreLoop` (MonoBehaviour) has `_coreLoopSM` (`0x20`), a `CoreLoopStateMachine` whose `_currentCoreLoopState` is at `0x70`.
- **Startup waits:** `Client.CoreLoop.InitState.InitAfterSceneLoaded` runs the setup steps (ItemBuyHelper, notifications, Firebase, AppsFlyer, Migration, CDNDownloadTest, IronSource, MRGS, `WaitGDRPCallback`, `InitUnityServices`, `InitResourceSystem`), then `Loading.LoadGameScene`.
  - `WaitGDRPCallback` (MoveNext RVA `0x1D86B9C`) loops until `InitState.OnGDRPCallback` (`0x38`) is true, with **no time limit**.
  - `InitUnityServices` (MoveNext RVA `0x1D86970`) loops until the `UnityServices.InitializeAsync` task completes, also with **no time limit**.
- **Loading window:** `Bowling.GUI.LoadingWindow` has these sections:
  - `_noConnectionSection` (`0x90`), `_connectionSection` (`0x98`), `_loginSection` (`0xA0`), `_onlineSection` (`0xA8`), `_offlineSection` (`0xB0`).
  - Buttons: `_btnReconnect` (`0xD0`), `_btnPlayOffline` (`0xD8`). `HandlePlayOfflineClick` just calls `FloatingMenu.switchToPlayNow()`.
  - Sections are swapped with the private `SwitchToSection(GameObject)` (RVA `0x1DDA484`).
  - `ConnectingWindowState`: Connecting 0, Disconnected 1, DisconnectedFirst 2, ConnectingFirst 3, Login 4, FacebookLogin 5.
- **When the offline button shows:** `HandleServerDisconnected` (RVA `0x1DDB334`) shows `_offlineSection` **only if** the static `Connector.WithoutReconnect` (statics `0xC0`) is true.
  - Otherwise (unless in COMPETE mode) it takes the reconnect path: `ReconnectManager.HasActiveRequest`, `ClientSideManager.IsPlayerIdSavedLocaly`, then `ConnectionModule.State`.
  - `Connector_serverStatusChanged` shows "No Network" or "Servers Unavailable".
- **NextDNS hang:** the user sees an endless loading loop with NextDNS (DNS over HTTPS) but a normal offline button with no internet [device]. The likely cause is a connection that keeps retrying without ever setting `WithoutReconnect`, though it hasn't been confirmed on device.
- **BowlingPlus 1.2.1 watchdog:**
  - After 30 s straight of the connecting/no-connection section with no offline/login/online section showing, it calls `SwitchToSection(_offlineSection)`.
  - If `OnGDRPCallback` is still false 20 s into `InitState` while no web page is on screen, it sets the flag to true.
  - The `InitUnityServices` wait has no fix: the task isn't reachable.
- `ConnectorTest` (with `CheckTimeout = 15000`, writing `connection_tests.result`) is a leftover developer test, not part of normal startup.
- **Game server** [files]: the only hard-coded server string is `s1.wannaplay.studio:4055` (Photon).
- **`Connector` statics** (global namespace) [files]:
  - `serverStatusChanged` `0x8`, `Client_API_Version` `0x48`, `Server_API_Version` `0x50`, `redirectTo` `0x58`, `_server_base` (`PhotonBase`) `0x68`.
  - `_state` (`ConnectState`) `0x70`, `peer` (`ExitGames.Client.Photon.PhotonPeer`) `0x78`.
  - `doReconnect` `0xA0`, `wasDiscByServer` `0xA1`, `_masterToken` `0xA8`, `_relays` (`Queue<string>`) `0xB8`, `WithoutReconnect` `0xC0`.
  - `ConnectState`: OFFLINE 0, MASTER_CONNECTING 1, MASTER_CONNECTED 2, GAME_CONNECTING 3, GAME_CONNECTED 4.
  - `PhotonPeer` has `get_ServerAddress`, `get_TransportProtocol` (`ConnectionProtocol` Udp 0, Tcp 1, WebSocket 4, WebSocketSecure 5) and `get_RoundTripTime`.
- **`ReconnectManager` statics** [files]: `reconnectCount` `0x8`, `reconnectSequenceEnd` `0xC`, `reconnectState` `0x10` (NO_RECONNECT 0, RECONNECTING 1, SPLIT_MONEY_NOTCASHED 2, RECONNECTED 3).
- **The game's `Logger`** [files]: a message prints only if its level is ≥ the logger's `_loggingLevel` (`0x18`) **and** ≥ the static `CommonMaxLoggingLevel` (statics `0x8`). `LoggingLevel`: None -1, All 0, Info 1, Warning 2, Error 3. Output goes through Unity's `Debug.Log`.
- **Console output** [files]: the game prints through `printf`, `fwrite`, `puts`, `write`, `NSLog` and `os_log`. BowlingPlus 1.2.2 captures stdout/stderr through a non-blocking pipe.
- **Live connection details** [device, 1.2.2 log with NextDNS]:
  - The connector reaches GAME_CONNECTED over **TCP** (`TransportProtocol` 1). The master server redirects it to `w1.wannaplay.studio:4056`, and the relays are `p1.wannaplay.studio:4056` and the raw IPv4 `51.20.120.110:4056`.
  - `Client_API_Version` = `Server_API_Version` = `0.25.86`, which is the number shown at the top of the loading screen.
- **Photon prefers IPv6** [files]: `IPhotonSocket.GetIpAddress` (RVA `0x32AD060`) uses `IPAddress.TryParse`, then `Dns.GetHostEntry`. It returns the **first IPv6** address (`AddressFamily` 23) and only uses IPv4 (2) when there's no IPv6 at all.
- **The game servers don't answer on IPv6** [device, NextDNS, two tests]:
  - DNS returned both families for `s1.wannaplay.studio` (204.74.248.116 / 2605:f480:9:1::).
  - TCP to port 4055 over **IPv4 was refused** right away, while over **IPv6 there was no answer at all** (6 s timeout).
  - IPv6 works fine to apple.com and to `api.wannaplay.studio`.
  - So any DNS that returns IPv6 for the game hosts makes Photon pick a dead address.
- **The game's DNS import** [files]: `UnityFramework` imports `getaddrinfo` (plus `gethostbyname` and `CFHostStartInfoResolution`) through a lazy slot at `0x481abf8` in `__DATA.__la_symbol_ptr`. It uses classic binding (`LC_DYLD_INFO_ONLY`), not chained fixups.
  - BowlingPlus 1.2.3 rebinds that slot fishhook-style, so `*.wannaplay.studio` / `*.spareball.com` lookups made with `AF_UNSPEC` return IPv4 only, falling back to the normal lookup if there's no IPv4.
- **Other network facts** [device]:
  - `clickhouse.wannaplay.studio` (the game's error reporting) doesn't resolve (NXDOMAIN).
  - `https://wannaplay.studio/` timed out twice.
  - With NextDNS on, iOS reports the path as "wifi + other" (a VPN-style tunnel).
  - AppLovin rewarded ads fail with `NSURLErrorDomain -1000 bad URL`.
- **The game's plain-HTTP requests are blocked by iOS** [device + Apple's rules]: a plain `http://api.wannaplay.studio/scripts` request fails in-process with **-1022 (ATS)**. Info.plist sets `NSAllowsArbitraryLoads`, but because `NSAllowsArbitraryLoadsInWebContent` and `NSAllowsLocalNetworking` are also present, iOS ignores it. So the game's `http://` endpoints (`api.wannaplay.studio/scripts`, `replays.spareball.com`) can't load through `NSURLSession`.
- **Practice lobby "apply"** [files]: `Bowling.GUI.PracticeOil.HandleApplyClicked` (RVA `0x1DFC8D8`) takes one of two paths:
  - **Plain path:** logs `practicelobby_apply`, then `Processing.Show()`, then `Managers.PracticeManager.PlayPractice` (RVA `0x1E7D5D0`). That builds a `Dictionary<byte, object>` and calls `Connector.Sender`, a Photon operation. The answer arrives through `HandlePlayPracticeUpdated`.
  - **Rewarded path:** logs `practicelobby_rewardedapply` and calls `IronceSourceIntegration.ShowVideo`.
- **The loading circle** [files]: `Processing` (MonoBehaviour) has `_instance` (statics `0x10`) and `_spinerCount` (`0x28`).
  - `Show()` activates it and adds 1.
  - `Hide()` subtracts 1; at ≤ 0 it deactivates and **resets the count to 0**.
  - There is no time limit.
  - [device] Once, after `practicelobby_apply` with NextDNS, it spun for 50+ s with no answer while the connector still said GAME_CONNECTED.
- **The game's logger level on device** is 2 (Warning) [device].
- **"Oops, it seems you are disconnected from the server."** with bouncing dots is a screen the user got stuck on with NextDNS and 1.2.1 [device]. That wording is **not** in the app: no code string, no shipped asset. It probably comes from server-delivered text.
  - `DIsconectByServerLogic.HandleDisc` uses the keys `disconnected_by_server_header` / `_body` for a **native Prime31 alert with an OK button**, which is a different screen from this one.
  - Which game window this screen is hasn't been identified yet. BowlingPlus 1.2.2 logs the open `FloatinMenuWnd` windows to find it.

## 7b. Privacy agreement popups [files + device]

- On a fresh install, a Korean agreement screen shows once and never again after "agree to all". That's MRGS's Korean privacy-law (PIPA) flow [device].
- `Client.CoreLoop.InitState.MRGSInitialization` (RVA `0x1D85158`) builds `MRGSIntegration` and calls `ShowUserAgreement(EUOnly = true, withAdvertising = false)` (RVA `0x1EA5F38`) on **every launch**. Decoded:
  1. `MRGSGDPR.onlyForEU(EUOnly)`, then `withAdvertising(...)`.
  2. If `getAgreedVersion() == -1` or `getAgreedVersion() >= _gdprVersion` (`0x30`), it calls `showAgreementFromFile(_appId, _gdprFullPath)` (`0x38`), the "Sign up" page.
  3. Otherwise it calls `showAgreementFromFile(_appId, _gdprUpdateFullPath)` (`0x40`), the "updated documents / I agree" page.
  4. So the game always asks; the SDK decides whether to actually show it.
- `MRGSGDPR` virtual slots (vtable base `0x128`, 16 bytes per slot): 21 `withAdvertising`, 28 `getAgreedVersion`, 31 `showAgreementFromFile`.
- The native bridge is `_mrgs_gdpr_get_agreed_version` and similar. Native storage classes: `MRGSGDPRStorage`, `MRGSGDPRExternalStorage`, `MRGSKeychainStorage`.
- The app's entitlements have **no** keychain access groups (team `MRJ5NFP78E`), so a re-signed copy keeps normal keychain access.
- In the user's sideloaded duplicate, the "Sign up" page shows on every launch [device]. The exact reason inside the SDK isn't verified; the server-side EU check failing for a re-signed app is a guess.
- The pages are `Data/Raw/MRGSGdpr/WPSmrgsgdpr.html` (WannaPlay Studio) and `GGmrgsgdpr.html`. The first-time page says "By clicking Sign up", and its button calls `clickButton()` → `document.forms["form"].submit()`. The `_update.html` pages say "By clicking I agree".
- BowlingPlus 1.2.0's opt-in auto-accept calls `clickButton()` in that WKWebView only when the page text contains "By clicking Sign up".

## 8. Arsenal list [files]

- `Bowling.GUI.ArsenalBallManager` has `_ballsData` (`List<mdl_Item_Ball>`, `0x90`) and `_ballsScroll` (`UISystem.UIDynamicScrollView`, `0x88`).
- `Init()` (RVA `0x1E0E9C8`) calls `InitScrollData()` (rebuilds and sorts `_ballsData`), then `_ballsScroll.ResetScroll()`. `GetCellsCount` returns `_ballsData` size.
- `UIDynamicScrollView.ReloadData()` (RVA `0x1D1881C`) only redraws cells already on screen. `ResetScroll()` (RVA `0x1D16A20`) recounts and resizes the list. After changing `_ballsData`, call `ResetScroll()`. Using `ReloadData()` gave the "empty space, rest of the Arsenal gone" bug [device, v1.0.1]. Switching to `ResetScroll()` fixed it [device, confirmed with 1.1.x].

## 9. Reference: names, offsets and addresses (1.907 build 1597)

Pulled straight from the dump. RVAs are offsets into `UnityFramework`. Every name below resolves on device [device: debug info `engine=1`].

```
RunPsycsTest: sphere 0x80, sphere_render 0x88, _kegsUp bool[] 0x68, kegsUpdate Action<bool[]> 0x28
  UpdatePinPositions() 0x1EBCFD4, RestartAfterTutorial() 0x1EC6524, switchToFun(bool) 0x1EC0E38, toTopScreenFromAny() 0x1EC0754
GameParams (static): gameMode 0x3C, _playerLocation 0x2C
PinHolder: _pins List<Pin> 0x20 | Pin: _physic 0x40, SyncObjects() 0x1EDF288
PinPhysic: _rigidbody 0x38, _centerOfMass 0x40, _inertiaTensor 0x4C, OnCollision 0x58
  Awake() 0x1EE0A08, FixedUpdate() 0x1EE0B10, OnCollisionEnter() 0x1EE0AEC
LoadDLCContentTexture: toChangeTexture 0x30, offset 0x38, dlcIDToLoad 0x40, objectID 0x44
  textureLoaded(Texture2D) 0x1CC7684, loadTextureForItem(item, sub, switchID) 0x1CC7920, loadTextureForItem(item, sub) 0x1CC79D8
LoadDLCContentTextureMaterial: material 0x20, dlcIDToLoad 0x28, loadTextureForItem 0x1CC7CD8, textureLoaded 0x1CC7C30
Models.mdl_Shop_Item: itm_id 0x10, itm_name 0x18, i_type 0x40, dlc_link List<int> 0x50, getDLC() 0x1CFD3A0
Models.mdl_Shop_Item_Ball: texc 0xE4
Models.mdl_Item: base_ide 0x10, item_id 0x14
Models.mdl_DLS_Item: dlc_id 0x10, dlc_type 0x14, dlc_sub_type 0x18, dlc_url 0x20, dlc_version 0x28, dlc_create_date 0x30
ItemsVisualData: _itemDatas 0x18, GetItemDataByID(int) 0x1CAFAC8, IsItemDataExist(int) 0x1CAFA20, TempFillVisualData() 0x1CC5838
  ItemsVisualData.ItemData (struct): Item 0x0, ID 0x8
Managers.DLCManager (static): cashed_dlc_config 0x0, all_server_dlc 0x10
DLCMemoryCash (static): loadedTextures 0x18, loadedVersions 0x28, getDLCForItemId(int) 0x1CB1CC8
DLCClientManager: loadItemWWW(url, id, cash) 0x1CAFE14, loadItemCashed(id) 0x1CAF73C
Managers.ItemHolder: _instance (static) 0x0, _shop 0x20, get_Instance() 0x1E4A5A8 (creates it!), GetShopItemById(int) 0x1E4E040
Managers.ShopItemHolder: _shopItems 0x10
Managers.InventoryHelper (static): _currentBall 0x10, get_CurrentBall() 0x1E4A73C
InventaryData: _instance (static) 0x0, ballsOnReturner 0x38, ballRenderer 0x50, _ballRenderersOnReturner 0x160
  InitBallsOnReturner(bool) 0x1DC317C, PrepareBallRenderer(GameObject, mdl_Shop_Item_Ball) 0x1DC2414, ResetRigidBody(GameObject) 0x1DC2040
Bowling.GUI.TutorialManager: Instance (static) 0x8, _activeStage 0x60, IsInTutorial() 0x1E46E94, SkipTutorial() 0x1E484A8
ClientSideManager: get_IsTutorialCompleted() 0x1CAC348, set_IsTutorialCompleted(bool) 0x1CAC520
Bowling.GUI.ArsenalBallManager: _ballsScroll 0x88, _ballsData 0x90, Init() 0x1E0E9C8, InitScrollData() 0x1E0EC34, Refresh() 0x1E0F430
UISystem.UIDynamicScrollView: _cellCount 0x16C, ResetScroll() 0x1D16A20, ReloadData() 0x1D1881C
```

Namespaces: the classes above with no prefix are in the global namespace. `ItemsVisualData`, `InventaryData`, `ClientSideManager`, `LoadDLCContentTexture` and `RunPsycsTest` are global too. `mdl_*` classes are in `Models`; `ItemHolder`, `ShopItemHolder`, `InventoryHelper` and `DLCManager` are in `Managers`.

## 10. Tooling that was used

- **Watch out when scanning the binary:** IL2CPP game code lives in a separate Mach-O section named **`il2cpp`** (VA `0x1c64798`, size `0x1f56174`), not in `__text` (VA `0x4000`, size `0x1c60798`). A scan of only `__text` silently finds nothing for game code. Scan the whole `__TEXT` segment.

- **Dump:** Il2CppDumper 6.7.x on `UnityFramework` + `global-metadata.dat`. It produces `dump.cs`, `script.json`, `stringliteral.json`, `il2cpp.h` and DummyDll. Disassembly was done with capstone; Unity assets were read with UnityPy 1.25.3.
- **Build:** Theos on Linux with L1ghtmann's clang toolchain and the iPhoneOS 16.5 SDK. `@available` can't be used because this toolchain lacks `___isOSVersionAtLeast`. `ldid` handles signing. The output dylib links only Apple system libraries (no Substrate), and its minimum iOS is 14.0.
- **Injection:** `tools/inject_ipa.py` copies the dylib into `Payload/<App>.app/Frameworks/` and adds an `LC_LOAD_DYLIB` for `@executable_path/Frameworks/BowlingPlus.dylib` to the main executable. The patched IPA installs and runs through Signulous [device].
- **Debug output:** the shake menu's **Copy debug info** button copies engine state, settings, current ball, the Match Up store entries (skin id, file, texc, shipped or not) and pin Rigidbody values. Ask the user to paste it; it's the fastest way to check the live data.

- **Building without Google or Maven (1.6.6, chats where only GitHub, PyPI, npm, crates and the Ubuntu archive are reachable):** see `tools/dev/android_offline_build.sh`. iOS: Theos from GitHub, L1ghtmann's `iOSToolchain-x86_64` (clang 11, with `ldid`) and `iPhoneOS16.5.sdk` from `theos/sdks` releases; builds cleanly. Android: `d8.jar` / `apksigner.jar` / a static x86-64 `zipalign` from the AndroidIDE `build-tools-34.0.4-x86_64` GitHub release, `android.jar` (API 35) from `Reginer/aosp-android-jar`, ZXing 3.5.3 compiled from its GitHub source, and the NDK **sysroot + compiler-rt/libunwind** (target code, host-independent) from termux-ndk r29, compiled with Ubuntu's clang-18 / lld-18 (`-rtlib=compiler-rt --unwindlib=libunwind -resource-dir=<ndk>/lib/clang/21`). The game's manifest has `extractNativeLibs="true"`, so the old zipalign's 4 KB `-p` is fine. Il2CppDumper's net6 build runs on Ubuntu's `dotnet8` with `DOTNET_ROLL_FORWARD=Major`.

## 11. Oil [files]

**Data**
- Patterns live in `OilDescriptionData` (ScriptableObject in the `data` bundle, `Assets/BundleData/Data/OilDescriptionData.asset`), field `Oils` (`List<OilDescriptionData.OilDescription>`, `0x18`).
- Each `OilDescription` has these fields:
  - Info: `LongName` `0x10`, `Id` `0x18`, `Category` `0x1C`, `Series` `0x20`, `Distance` `0x24` (ft), `Volume` `0x28` (mL), `Ratio` `0x2C`, `Color` `0x30`.
  - Grids: **`OilMatrix` (`float[,]`, live, `0x40`)**, `_sourceOilMatrix` (clean copy, `0x48`).
  - Source file: `_source` (`TextAsset`, `0x50`).
- `GameParams` statics: `OIL_MAP_WIDTH` `0x50`, `OIL_MAP_LENGTH` `0x54`, **`OIL_SELECTED` `0xA8` (1 + list index)**.
- A `float[,]` is stored with its bounds pointer at array `+0x10` (`dim0` at bounds `+0`, `dim1` at bounds `+0x10`) and its data at `+0x20`, row-major `[board, row]`.
- 48 patterns ship with the game. All are real **Kegel lane-machine files**: the Kegel Challenge/Recreation/Sport/Element series, the KSS series, USBC and ABC championship patterns, and 10 landmark house patterns.

**Pattern file format** (text, one value per line; checked on "ROUTE 66"):
- id `-1`, name, `<p>` notes, then machine settings.
- Then 8 slots each for: forward start boards, forward stop boards, forward loads, forward speeds, reverse start boards, reverse stop boards, reverse loads, reverse speeds, forward end footages, then the L/R text versions.
- **Boards are absolute 1–39 from the left** (38 = 2R).

**The Kegel engine** (namespace `Kegel`)
- `Drawer.drawFromFile(TextAsset)` does `mPattern = new Pattern(); mPattern.OpenPatternFromFile(txt)`, allocates `Units` (static `0x10`), then runs `Graph3DForward()` and `Graph3DReverse()`.
- `Pattern` has `mReverse` `0x18` and `mForward` `0x20`, both `PatternLoadScreens : List<LoadScreen>` with `Add(int start, int stop, int loads, int speed, float ef)`.
- `LoadScreen` fields: `mStart` `0x10`, `mStop` `0x14`, `mLoads` `0x18`, `mSpeed` `0x1C`, `mEndFootage` `0x20`, `mDirection` `0x24`, `mPrevious` `0x28`, `mOldMath` `0x38`.
- **New math** (`Add` sets `mOldMath = false`), forward:
  - First step: end = `(loads − 1) × speed × 17/12 / 10` ft.
  - Later steps: end = `previous end + loads × speed × 17/12 / 10` ft.
  - **Zero-load (travel) steps use the given `ef`.**
  - Reverse steps subtract instead. This reproduces Route 66's file footages within Kegel's 0.1 ft truncation.
- `OilDescription.Parce()`:
  - First time: allocates both grids at `OIL_MAP_WIDTH × OIL_MAP_LENGTH`, runs `drawFromFile(_source)`, then sets **`OilMatrix[w, h] = Units[w + 1, h / 4]`** and copies it to `_sourceOilMatrix`.
  - Later calls: copies `_sourceOilMatrix` over `OilMatrix`, which is the "fresh oil" reset.
  - Callers: `ParceOils` (startup) and `OilMapGenerator.ReloadAndRedrawOil` (from `InfoScreenManager.reStart` (new game), `H2HManager.applyOilToLane`, and demo replays).
- `TextAsset(string)` is stripped, so a new pattern file can't be made. Build it in memory with `Add` instead.

**Breakdown and carrydown**
- `BallSoundManager.OnCollisionStay`:
  - `board = round((x + W/2)/W × OIL_MAP_WIDTH)`, with W from `GameParams` `0xD8`; the row is computed the same way down the lane.
  - Friction comes from `OilMatrix[board,row]` via `OilHeightToFriction.getFriction`.
  - Then it calls `redistributeOil(board,row)`.
- `redistributeOil` walks from the previous contact cell (`0x100`/`0x104`) to the current one. For each step, `redistributeOilOnLaneDspl(w1,h1,w2,h2)` moves `src × displacement_percent` from cell 1 to cell 2 (static `0x8`, **= 0.2** from the `.cctor`), capping cells at 99.99.
- **Physics is self-consistent:** the friction read and the oil moves use the same cells.

**Drawing (the carrydown "mirror" bug)**
- `OilMapGenerator` (MonoBehaviour) has `Lines` (`List<Renderer>`, `0x28`) and statics `_oilMaps` (`OilTexInfo {Id, Texture, TextureData}` structs) and `_colorData` (`OilColorData {Gradient OilGradient 0x18, float MaxHeight 0x20}`).
- `GenerateRG16Texture` creates `Texture2D(w, h, RG16)` with **wrapMode Clamp (1)** and bilinear filtering.
- `ReDrawOil` shows oil only if a game setting allows it **and** player level ≥ 3. `ReloadCurrentOilMap` redraws if the live grid changed (it's called from `RunPsycsTest.SelectBall`).
- The lane material is `assign_wood_only_oil_hsv`, with shader "Transparent/Diffuse Bowling Oil Recolored" (properties `_MainTex _OilMap _SizeX _SizeY _OilHue _BallHUESize`). Its decompressed Metal code reads `u = POSITION.x / _SizeX; TEXCOORD3.x = 1.0 − u; TEXCOORD3.y = POSITION.y / _SizeY;`.
- **The `1 − u` mirrors the oil against the physics.** The game never sets `_SizeX` at runtime (no code string).
- **[files] Per-axis wrap setters are stripped:** the game's metadata (iOS and Android) has `set_wrapMode` but no `set_wrapModeU` / `set_wrapModeV` / `get_wrapModeV`. So Repeat for the mirror fix also wraps along the lane: at the lane's far edge (v = 1) bilinear filtering blends the last row with row 0, the heavy oil at the foul line, which drew a line behind the pins [device, 1.6.1]. 1.6.2 scales `_SizeY` by rows / (rows − 0.6) so the far edge samples inside the last row (checked against the row-picking arithmetic for 60 to 480 rows). `Texture.get_height` survives. `_SizeY` is a shader property, so it isn't in the metadata; it's checked with `HasProperty` at runtime.
- Fix used by BowlingPlus 1.3.0: negative `_SizeX` plus the oil texture on Repeat. That gives `u = 1 + x/S`, which wraps to `x/S`. The original only works with Clamp if the mesh spans `0..S`.

**More engine facts** [files]
- Both forward and reverse zero-load steps use the stored `mEndFootage` (`ef`).
- Graph3DForward and Graph3DReverse do **not** read `mDropBrushInReverse` (`0x9C`), `mFVPB` (`0xBC`) or `mRVPB` (`0xC0`). Only `LoadScreen.get_TotalOil` (the "T.Oil" number) and the parsers use them. So the drawn oil depends only on the load steps (boards, loads, speed, travel footage).
- The new math doesn't round. Kegel sheets show each step's distance cut to 0.1 ft (1 load × 18 in/s = 2.55 ft, shown as 2.5), so engine footages can differ from a printed sheet by a few tenths of a foot on long runs of steps. Example: the 2012 USBC Open Baton Rouge reverse pass lands at 14.35 / 11.8 / 9.25 / 6.7 ft vs. the sheet's 14.4 / 11.9 / 9.4 / 6.9 ft.

**Kegel pattern files** [files: 2026 PBA Regional 37 zip from the Kegel Pattern Library]
- **The zip holds `.Pattern`, `.txt` and `.pdf`** (+ `__MACOSX` copies).
- **`.Pattern` is JSON:** `Name`, `Distance`, `ReverseDropBrushDistance` (30), `ForwardDropBrushDistance`, and `ForwardLoadscreens` / `ReverseLoadscreens`. Those lists have 15 slots each, with unused ones zeroed. Each slot holds `Start`, `Stop`, `Loads`, `SpeedIps`, `TankA`/`TankB`, `Microliter`, `BuffSpeed` and `EndDistance` (2 decimals, e.g. 3.92).
- **`.txt` is exactly the game's built-in pattern format.**
  - Header lines: name at line 1, µL at 10, distance at 12, line 13 = 29 here (the `.Pattern`/PDF say drop brush 30).
  - 15-line columns: forward start / stop / loads / speed at lines 14 / 29 / 44 / 59, reverse at 75 / 90 / 105 / 120, forward ends at 136, reverse ends at 211, then L/R text.
  - Checked against the game's Route 66 file too.
- **Regional 37 is lopsided** (2L–6R, 4L–9R, 11L–10R, 13L–12R). Volume 32.65 mL = (373 + 280 boards crossed) × 50 µL.

**Correction (device, 1.4.7 debug info): the game already shows each pin's random turn.**
- The debug info showed "pin turn: pins turned=0 axes=0": the 1.4.5 feature never activated, because the body's up axis never reached `y > 0.99`.
- Yet the user's screenshot with a two-sided pin picture showed standing pins facing different ways.
- The game's default pin picture looks the same from every side, which is why racks *looked* lined up.
- The feature was removed in 1.4.8. The `SyncObjects` reading below was wrong about the visual dropping the turn.

**Pins: why they all face the same way** [files]
- **Prefab** `PinRuntimeObject` (prefabs bundle):
  - `PinPhysic` (Rigidbody; capsule + mesh colliders), with `PinVisual` under it (and `PinHighQuality` / `PinLowQuality` under that, MeshRenderers with identity local rotation)
  - `PinMirror` (the lane reflection)
  - `PinShadowBlob`
- **`Pin` fields:** `_objVisualRoot` `0x28`, `_highQualityRender` `0x30`, `_lowQualityRender` `0x38`, `_physic` `0x40`, `_shadowProjector` `0x48`, `_mirrorRenderer` `0x50`.
- **The pinsetter already turns pins randomly.** `PinHolder.SetupPins` (called from `RunPsycsTest.UpdatePinPositions` / `DisablePhysicObjects`) gives each pin's body a random turn: `Random.Range(0, 16)` × 22.5°. **(Corrected in 1.6.6, see section 5a: on this build `UpdatePinPositions` turns the kegels itself; `SetupPins` only runs when `UsePinHolder` is on.)**
- **[device, 1.6.1] The turn IS visible with a pin image**, and it changes whenever the game racks: picking up a ball re-racks, so pins showed a new random turn on every pickup. (1.4.8 found the game already shows each pin's turn; its stock pins just look the same from every side. The line below is from before that.) 1.6.2 tried to keep each pin's turn for the frame by setting the PinPhysic transform's rotation back. **[device, 1.6.2] It never acted:** `kept=0 newFrames=0 ball=4 knocked=0` after 4 throws, pins still turning. A simulation reproduced those numbers from two mistakes: it required a lean under 1° from the saved pose (a settled pin vs. a fresh exactly-upright rack exceeds that), and it keyed saved poses by holder slot while re-fetching the holders every 120 frames in Unity's unordered `FindObjectsOfType` order. 1.6.3: per-pin-object keys, 6° standing / 15° knocked, and only the twist about world up is undone (swing-twist split), keeping the pin's current lean. **[device, 1.6.3] Still turning**, and the new counters showed why: `holders=1 pins=10 readFails=0 | kept=0 … maxTilt=0 | turned between checks: body=0 visible=0` over 3 ball pickups (no throws). The `PinHolder._pins` set reads fine but never moves. **The pins on the lane are `InventaryData.kegels`**: `InventaryData` has `kegels kegels_renderers(_h/_l) pinseterKegelRenderer pindeckKegelRenderer _pinHolder UsePinHolder` (metadata), i.e. two pin systems chosen by `UsePinHolder`. 1.6.4 watches both. **[device, 1.6.4] Confirmed the kegels:** `UsePinHolder=0`, `kegels=10 (UnityEngine.GameObject[])`, turns put back (`kept=66`), PinHolder pins never turned. **Pins still visibly turned**: BowlingPlus's iOS timer runs at 60/s (`preferredFramesPerSecond = 60`) and the pin check ran every 3rd tick, so up to 50 ms (6 frames at 120 FPS) passed before a turn was put back. 1.6.5 runs the pin check from a second display link at the screen's rate (Android: every frame already). **Seeding the turn instead is impossible:** `UnityEngine.Random` keeps only `Range` and `RandomRangeInt` in the metadata, and the engine (iOS UnityFramework, Android libunity.so) has no `UnityEngine.Random::InitState` / `get_state_Injected` / `set_state_Injected` icall names left (`il2cpp_resolve_icall` itself is exported). Also: **picking up a ball passes through `LOC_THROWING`** (device: `ball=3` counted with no throws), so a throw is now "ball body faster than 1 m/s while THROWING".
- **The visual drops that turn.** (superseded, see above) `Pin.SyncObjects` (every update, from `PinHolder.SyncPins` ← `RunPsycsTest.Update_4`) sets the visual root to `FromToRotation(up, body.up)` × a fixed rotation, then the mirror from that. So every pin is drawn facing the same way, and flying pins never spin around their own axis.
- **1.4.5 "Pins face random ways":**
  - Turns the two mesh children (which the game never rotates) by the body's twist, `θ = 2·atan2(q.y, q.w)`, around the pin's axis measured once while it stands. That axis is `inverse(visual root) × body up`.
  - The result is exactly the body rotation × the game's mesh offset (checked: worst 0.1° over 2000 random poses, float rounding).
  - The reflection gets the same turn around world up while the pin stands.
  - Physics is untouched.

**Pin image** [files]
- **Materials:** `PinHighQuality` and `PinLowQuality` share the material **`pin_diff`** (Legacy Shaders/Reflective/Diffuse: `_MainTex` + `_Cube`). `PinMirror` uses **`keg_d`** (Mobile/Diffuse), with the same `_MainTex`.
- **The game's own pin image** is **`pins1`**, 512 × 512, in `Raw/AssetBundles/pins/fulltextures`. That bundle holds 178 pin images (the Pin Arsenal designs, e.g. `tex_pin_rainbow`, `kegl_blue`), all 512 × 512.
- **Mesh** `polySurface060` (652 vertices): the pin stands along Y from 0 to 0.381 m (15 in).
- **UV layout**, found by painting every triangle into a 512² map of height and angle:
  - **Two half-pins side by side.** The left shape and the right shape are opposite sides; each one's middle line is the middle of that side (angles 270° / 90°), and their curved edges meet.
  - **Head at the top**, base at the bottom.
  - **The pin's underside** is the disc in the middle.
  - About 36% of the image is unused.
- **Checked by rendering the mesh** with the guide wrapped on it: Side A and Side B land centered on opposite sides.
- **More pin models** (level1): besides the 10 real pins there are "keg" models (mesh `polySurface62`, material `keg_d`):
  - under the pinsetter's joints (`pinspotter_1:jointNN`, the pins it lowers for a new rack)
  - under `MirrorRender`, `MirrorRenderOnPindeck` and `MirrorRenderArsenal`
  - `PinSetterManager.mirrorOnPinsetter` lists 10 of them.
- **`InventaryData`** holds lists of them all: `kegels`, `kegels_renderers`(`_h`/`_l`), `mirrorPinsRender`, `mirrorPinsPindeck`, `pinsArsenal`, `pinsArsenalMirror`, `pinseterKegelRenderer`, `pindeckKegelRenderer`.
- **`ApplyTexturePins`** (at each mode switch) does `renderer.material = m` with `pinMaterial` (`0xD8`) or `mirrorPinMaterial` (`0xE0`).
- **[device, 1.4.6]** Only the real pins' materials got the image, so each new rack came down with the game's pins and then "popped". Since 1.4.7 those two materials get it too, and changed materials are re-checked every frame.
- **Other models' layouts:** the low-quality pin (`polySurface60`) and reflection / pinsetter model (`polySurface62`) use the same layout, with rougher edges (1–2 px at 512).
- **The shader** is Reflective/Diffuse, which uses `_MainTex` alpha as its shininess mask. `pins1` is fully solid (alpha 255).
- **[device, 1.4.6] Crack down the pin's side** where the two halves meet. Mipmap/filter blending picks up pixels outside the shapes.
- **Since 1.4.7, a saved picture is:**
  1. drawn on white
  2. filled outward from the shapes (shrunk 2 px: `src/PinMask.h`, 512² bits)
  3. made fully solid.
- **Checked** by running the same C fill on a gray-surrounded picture and rendering a far-away pin: the gray seam line is gone.
- **[device, 1.4.7] Crack near the base.** The user's hand-drawn square layout had its shape outline slightly inside the real shapes (lower edge of side A), leaving slivers of its dark background on the pin.
  - 1.4.8 `PinFillAround(cleanEdges)` refills a 6 px band at the shape edges plus background-colored pixels within 16 px. The background is the most common color outside the shapes.
  - Checked on that picture: background-colored pixels in the base band went from 2165 to 1.
- **[device, 1.4.7] Crown spikes doubled at the seam.** Each half-pin shape is a side projection, so the image squeezes the surface near its curved edges. Spacing drawn evenly in the image isn't even on the pin, and the two halves' edges don't line up.
- **1.4.8 wrap layout** (a 2:1 picture):
  - Left to right is once around the pin, angle `u = ((720 − atan2(z, x)°) mod 360) / 360`, so side A (270°) = ¼ and side B (90°) = ¾, and the edges meet. Top to bottom is height 0.381 m → 0.
  - `PinWrapToLayout` (`src/PinWrap.h`) rasterizes the mesh (`src/PinMesh.h`: 652 corners, 1150 triangles) into the game layout and samples the wrap at each texel's 3D point. Both halves agree at the seams, and the flat underside keeps the template.
  - Checked by rendering: text reads correctly (not mirrored, after flipping the direction once), and 8 evenly spaced spikes stay continuous across both seams.
- **1.4.6 "Use my own pin image":**
  - The picked picture is saved as a square PNG (512–2048 px) at `Documents/BowlingPlus/pin_image.png`, and loaded with `Texture2D(2,2)` + `ImageConversion.LoadImage`.
  - It's set as `mainTexture` on every pin renderer's shared material (all 4 lanes, reflections included), with the game's texture kept per material.
  - It's re-applied when the game swaps pins (Pin Arsenal); the switch off puts the game's back.
  - The guide and a 1024² template of `pins1` are embedded (`src/PinGuide.h`) and shared from the menu.

**The game's 48 built-in patterns and their files** [files: `Data/Raw/AssetBundles/data`]
- **Source:** every `OilDescription` has `_source` (`0x50`), a `TextAsset` with the Kegel file. `TextAsset.get_text` reads it on device.
- **Layout:** the same for all 48 (`src/KegelParse.h`): line 10 µL per board, 12 distance, 13 reverse brush drop, 15-line columns for forward start 14 / stop 29 / loads 44 / speed 59 / end 136 and reverse 75 / 90 / 105 / 120 / 211.
  - **Line 0** is `-1` in 40 files and a pattern id (205, 67, 1640…) in 8 (the 2020 Kegel series, Tower of Pisa, 2004 ABC Nationals). The first importer wrongly required `-1`.
  - **Name:** line 1 holds the name in `-1` files and the series in the others (the name is then on line 2).
- **Checks against the game's own `OilDescriptionData` numbers:**
  - Distance: 47 of 48 match the file. Great Wall's label says 48 but its file and its last step say 42, so the label is a typo.
  - Volume (`Σ loads × boards × µL`): 32 of 48 match exactly. The other 16 are hand-tuned patterns whose recorded volumes are all about 25 mL whatever their steps add up to (Chichen Itza matches at 50 µL though its header says 40). The file is still what the game's engine builds the lane from.
- **Left-right:** 36 of 48 are exactly symmetric and the other 12 differ by at most 2.4% of their oil, so flipping to Kegel orientation changes little.
- **1.6.0** applies the Kegel-accurate model to all 48 in Practice, with each file's ends used as given (`precise`): each pattern's `_sourceOilMatrix` and live `OilMatrix` are swapped, the originals are kept, and everything is restored whenever `InPracticeOil()` is false.
  - **Restore order:** a custom pattern on top is restored first.
  - **Redraw:** lane pictures are redrawn at 6 per call, the selected pattern first.
  - **Totals** against the game's engine on the same file: 0.72x to 1.19x. Max cell 83. No holes in the film and nothing past the distance.
  - **Repeat the check:** `tools/oil_verify/extract_game_patterns.py` + `all48.cpp`.

**The privacy page, once** [files + earlier device logs]
- **Page:** the SDK shows `WPSmrgsgdpr.html` in a `WKWebView` on every launch of a sideloaded copy. Its button calls `clickButton()`.
- **1.6.0 flow:** watch the first-time page ("By clicking Sign up"). When it goes away you accepted: `privacyOK = true`. Afterwards each new web view is concealed on sight (30 Hz timer; `alpha = 0` on the top-most piece that isn't the game's own root view, or the whole window if the SDK uses its own), probed, and either pressed (Sign up page) or revealed (anything else, or after 8 s). Only in the first 5 minutes of a launch.
- **Untested on a device:** the concealing and the first-accept detection.

**The game's pin layouts** [files; the tap detection is untested on a device]
- **Top right (holding the ball):** `Managers.Bowling.MainMenuButtonMan` has `pinObj` `0x70`, `pinObjBack` `0x78` and `pins` `0xC0`, updated by its `HandleUpdatePinAvailables(bool[])`, which listens to `RunPsycsTest.kegsUpdate`.
- **Overhead of the ball return:** `ObjectsToShift/ObjectsOnSmallMonitor/MonitorCollider` (BoxCollider, layer 11) is the little screen. It has no script. `BackFromMonitor` next to it is an `EventTrigger` on a huge collider behind the scene ("tap anywhere else to go back"). `RunPsycsTest.renderToTexMonitor` (`0x30`, `RenderToTextureOnce`) redraws the screen's picture.
- **Locations:** `LOC_UPPER_SCREEN 0, BALL_RETURNER 1, BOTTOM_MONITOR 2, START_POS 3, THROWING 4, ON_PINDECK 5, REPLAYER 6`.
- **1.6.0 hit-testing:** a tap recognizer on the game's window (touches still reach the game), the tap as 0..1 of the screen, and
  - at `START_POS`: `GetWorldCorners` of `pinObjBack` / `pinObj` (through the canvas camera unless it's an overlay canvas)
  - at `BALL_RETURNER` / `BOTTOM_MONITOR`: the 8 corners of `MonitorCollider.bounds` through the highest-depth enabled camera that draws its layer

  Both rectangles get about 12% extra room. `RunPsycsTest.UpdatePinPositions` with an edited `_kegsUp` (the same call spare mode uses) re-racks at any time.
- **Debug:** "Copy debug info" has a `pin tap:` line with the last tap, the rectangle and the decision.

**Kegel's chart, measured (1.5.3)** [files: 2025 U.S. Open #4, 2017 SEA Games Long, 2026 Regional 37, Start 5 / Stop 15 calibration; 400 dpi renders]
- **Geometry the chart draws:** every pass is a rectangle at *exact* distances, not the sheet's rounded numbers.
  - **Per load:** each load travels `speed × 0.14` ft (14 in/s → 1.96 ft). This reproduces every end distance in Kegel's `.Pattern` file for Regional 37, forward and reverse.
  - **Forward:** the first step counts `loads − 1`; a step covers `[previous end, end]`. Zero-load steps are travel to the sheet's integer.
  - **Reverse:** the chain starts at the first (travel) step's end (e.g. 38→37 starts at 37), and each loaded step subtracts `loads × speed × 0.14`.
  - **Example:** the sheet's "10 → 14" is really 9.80 → 13.72.
  - **Older sheets** (Baton Rouge, 2012) use 17/120 ft per speed unit instead.
- **Layers:** each pass is a translucent rectangle (forward darker than reverse; forward + reverse darker still), with *constant* opacity per pass type: it does not depend on µL or loads. So the chart's shades show *which* pass touched the lane, not thickness.
- **Film layer:** underneath, a brushed film covers every board 2–38 (not 1 or 39), including boards the oil head never crossed, from 0 to the pattern distance.
  - **Darkness:** about 79 at the foul line, fading roughly linearly (~0.93 per ft), then a lighter tier from the reverse brush drop to the distance (about 26 → 21 on U.S. Open #4). It ends at the distance.
  - **Beyond the distance:** nothing.
- **Final travel to the foul line:** the chart draws the last loaded reverse step's rectangle continuing down to 0 over that step's boards.
- **Calibration:** 5-ft lines at y = 245.5 + (55 − ft) × 45.2 px; board columns at x = 59.5 + (b − 0.5) × 21.86 px; an offset of about +0.1 ft at the foul line.
- **Result:** the model agrees with the chart on 99.90% of 23,088 cells (4 patterns); the remaining cells are the chart's black arrow triangles (`tools/oil_verify/`).

**Why the game's own oil model (and BowlingPlus 1.5.0–1.5.2) mismatched the sheet** [files + device: 2025 U.S. Open #4]
- **Rounded whole-foot rows.** The engine places each step on 1 ft rows from the rounded footage, covering `[previous end + 1, end]` inclusive, which makes adjacent forward / reverse steps overlap by a row and shifts steps by up to a foot.
- **No film on boards the head never crossed.** Its carry model only runs after a board's last oiled foot, so boards 6–7 and 33–34 of U.S. Open #4 stay bare. The sheet has film there.
- **A single flat carry level** instead of the sheet's two film tiers.
- **Other model departures:** reverse oil doubles (`2 × old + new`), µL per step ignored.
- **1.5.2 on top of that:** it rescaled everything to the game model's total (×1.15 here, ×1.8 on SEA Games Long), inflating every pass.
- **1.5.3** draws on the lane's real 0.25 ft rows (`OIL_MAP_LENGTH` 240 = 60 ft × 4) with exact distances, film everywhere and no rescaling.
  - **Film:** 25 game units at the foul line, `× (1 − 0.01177 ft)` before the brush drop, then `0.6 ×` that value at the drop fading to `0.27 × 25` at the distance.
  - **Passes:** `339 / speed × µL / 50` per foot, added.
  - **Reverse:** counts only below the brush drop.

**Friction vs oil height** [files, partly]
- `OilHeightToFriction.getFriction(int oilHeight)` is a lookup table `float[] friction` (`0x18`), one entry per 5 oil units, linearly interpolated (index = h / 5, remainder h mod 5), clamped at the ends. It is called from `BallSoundManager.OnCollisionStay`.
- The tables live in `BallOilHeight.oilToFriction[]` (one per ball polishing). The asset instance wasn't located, so the numbers aren't read yet.

**What Kegel's lane chart shades mean** [files: 2017 Southeast Asian Games – Long, Baton Rouge]
- The chart on a Kegel sheet shades the lane **by which pass touched it**, not by oil thickness. The Baton Rouge sheet's legend says so: Forward, Reverse, Combined, Buff.
- On the SEA Games sheet, the 4 shades are:
  - forward oil (darkest)
  - reverse-only oil
  - brushed (buffed) up to the 39 ft drop brush
  - 39–44 ft
- **Real thickness can be the opposite.** At 10–19 ft the outside boards get only the reverse 5 loads × 50 µL over 9 ft (T.OIL 9,250), heavier per foot than 0–10 ft (5 × 35 µL over 10 ft, T.OIL 6,475), yet the chart draws 0–10 ft darker.

**The game's oil model vs Kegel's** [files + device 1.4.9 oil report]
- **Forward oil per foot, `339 / speed`, is Kegel's own density** for 50 µL a board: 50 / (speed × 17/120) = 353 / speed.
- **Where the game departs:**
  - reverse `2 × old + new` (Kegel: `old + new`)
  - µL per step ignored (sheets use 35, 40, 50…)
  - the last travel step to the foul line lays oil (sheet: T.OIL 0)
  - boards keep nearly full oil past their last oiled foot (the carry model)
- **On SEA Games Long** this makes 0–19 ft one thickness (≈ 60–62) and the outside 20–44 ft ≈ 10–15, so only 2 visible shades. Placement was right: step ends matched the PDF, and the self-check showed 0 differences.
- **1.5.1 "Kegel-accurate oil"** (custom patterns, on by default) uses:
  - `339 / speed × µL / 50`
  - reverse adds
  - no oil from travel steps
  - a thin film (loads so far × 1.5 × µL/50, the game's own carry end level) past each board's first oiled foot wherever the head didn't oil
  - the result scaled to the game model's total for the pattern, because the game's friction is tuned to its amounts
- **Checked on SEA Games Long:** distinct levels 12 / 17 / 30 / 59 / 67 / 72 / 85 instead of 2. The shipped function matches the prototype exactly.
- **[device, 1.5.1] The front of the lane came out too thin** (0–10 ft = 34 vs 79 at 10–19 ft).
  - 1.5.1 had dropped the game's "travel to the foul line lays oil" step as a quirk. It is the **reverse brush carry-down**: after the last reverse load the machine travels back with the brush down and wipes that oil onto the front. The sheet's T.OIL 0 means the pump doesn't fire, not that no oil lands.
  - 1.5.2 puts it back in the Kegel-accurate model: `+ 339 / speed × µL / 50` over the last loaded reverse step's boards, from that step's end to the foul line.
  - **SEA Games Long after the fix:** front 59, center 10–19 ft 68, outside 10–19 ft 54, pyramid 57–58, brushed film 10–23. This matches the PDF chart's layout (front and center darkest, outside 10–19 ft one shade lighter, the pyramid, light outside).
- **Steps now carry µL** as the 6th value: the PDF MICS column, `.Pattern` `Microliter`, the `.txt` header for every step, and 50 for older patterns.

**Ball spin (RPM)** [files]
- **At release,** `RunPsycsTest.FixedUpdate` applies the throw's spin once:
  - The spin is `RunPsycsTest.rotation` (private `Vector3`, `0x1BC`, in rpm).
  - It's applied as `AddTorque(rotation × 2π / 60, ForceMode.VelocityChange)`. The constants are `0x40490FDB` = π and `0x42700000` = 60, in the same block that writes the "mph" text.
  - The scoreboard's rpm `Text[]` is `RunPsycsTest.rpm` (`0x178`). `Constants.MAX_RPM` is static `0x8C`.
- **The hook** then comes from the physics engine: friction of the spinning ball on the lane, with oil friction from `BallSoundManager`.
- **The spin limit:** `InventaryData.ResetRigidBody` sets the ball's `maxAngularVelocity` (and `maxDepenetrationVelocity`) to 100,000 rad/s (`0x47C35000`).
- **The 600 rpm cap** comes only from the throw input.
- **Grip formula** (`BallSoundManager.OnCollisionStay`, every physics step on the lane), written into the lane `PhysicsMaterial`'s static and dynamic friction:
  - `friction = OilHeightToFriction.getFriction(oil) × min(|ω|, BallMaxOmega) / BallMaxOmega × InventaryData.rpmFactor (0x24) × (wear term: GameParams.getBallValueDurability, …)`
  - `BallMaxOmega` = `_ballMaxOmega` (`0x170`), filled on first use as `ballMaxRmp` (`0x20`) × 2π / 60.
  - **Spin above `BallMaxOmega` adds no grip.** [device, 1.4.9] At 17x the ball spun visibly faster and showed 10,200 rpm but hooked the same.
  - The middle settings looking slower is the wagon-wheel effect (a few revolutions per screen frame).
- **1.5.0 fix:** for a boosted throw, `rpmFactor` is raised by `max(1, boost × release spin / BallMaxOmega)` while that ball rolls, which is the friction an uncapped formula would give. It's restored as soon as the ball leaves `LOC_THROWING` (or the boost is turned off). Practice only.
- **1.4.9 spin boost:** right after release (`LOC_THROWING`, ball speed > 1 m/s), the ball's `angularVelocity` is multiplied once (1–17x, same axis), never above `maxAngularVelocity`. The rpm text is scaled too. Practice only.

**Networking: the IPv4 filter was never attached** [files + device logs]
- Every debug log showed `dnsSlots=0`.
- **Why:** the main executable doesn't link UnityFramework. Unity's iOS template loads it from `main` on demand, after BowlingPlus's start-up scan of loaded images.
- **The import:** UnityFramework imports `_getaddrinfo` through `__DATA.__la_symbol_ptr` `0x481abf8` (indirect symbol 13923).
- **The fix (1.4.8):** attach with `_dyld_register_func_for_add_image`.
- **[device, another user, iOS 15.1]** The App Store game was stuck at 0% loading too.
  - Connection test: DNS fine, but TCP `s1.wannaplay.studio:4055` over IPv4 got "Connection refused" in 77 ms, so something on that network actively rejects the game's server port.
  - Their launches stuck loading also tripped BowlingPlus's safe mode. Since 1.4.8 a launch still running after 60 s resets the crash guard.

**Kegel PDFs** [files: "Start 5 And Stop 15 Calibration Pattern" from the Kegel website, 2026 Regional 37]
- **The Kegel website and app download only the PDF data sheet.** The PDF Title is the pattern name, and the producer is "KEGEL Pattern Library".
- **Its text** (PDFKit / pdftotext reading order):
  - Values come first: "36 FEET" (distance), then "20 FEET" (drop brush).
  - Then one line per step under "FORWARD LOADS DATA" / "REVERSE LOADS DATA": `# START STOP LOADS MICS SPEED BUFF TANK from →to T.OIL`. Tank text varies ("A -", "A -FIRE").
- **Distances are whole feet**, Kegel's rounding of the exact value (3.92 → 4, 22.84 → 23). For Regional 37 these give the same engine rows as the `.Pattern` file's exact distances.
- **Board notation:** the calibration pattern writes boards right-to-left ("15R → 5R"), so boards are ordered low to high on import.
- **Checked layouts:** the parser was checked on both PDFs, with the text also re-split one cell per line, all on one line, and without the "REVERSE LOADS DATA" heading (then the row numbering restarting at 1 marks the reverse table).

**Two things the game's engine gets wrong for real Kegel patterns** [device + files]
- **Bare gaps.** The transfer fill only runs after a board's *last* oiled foot. When a pattern narrows and then widens again (Regional 37: 7L–7R at 6–12 ft, then 4L–9R at 12–16 ft), the boards in between stay at 0.
  - The engine copy reproduces the user's bare square exactly: boards 4L–5L at 11–12 ft.
  - 1.4.3 filled those feet by blending the oil on either side. That turned the calibration pattern's alternating 2 ft bars into solid blocks.
  - Since 1.4.4, a skipped stretch gets a thin buffed film instead, like Kegel's chart: the board's loads so far × 1.5 (the level the game's own transfer ends at), capped by the oil on either side. Calibration: 24.2-unit bars with 1.5 / 3 / 4.5 / 6-unit films between. Regional 37: 4L–5L at 11–12 ft = 6.0.
- **Sides.** The game's grids have lane column 0 on the bowler's **right** (`OilMatrix[w] = Units[w + 1]`, physics and, with the mirror fix, display). So Kegel's left boards show on the right.
  - Evidence: the device preview (top edge = column 0) and the lane showed Regional 37's left-side features (bare square at 4L–5L) on the right.
  - Since 1.4.3, custom patterns put Kegel board n in column 39 − n, and the preview's top edge is the bowler's left.
  - The game's own patterns are untouched. Its asymmetric ones play mirrored versus their Kegel sheets.
- **The self-check** (oil report) still compares the game's way (no fill, game sides).

**Why custom patterns came out wrong, and the 1.4.2 fix** [device + files]
- **The 1.4.1 oil report on device:**
  - Baton Rouge's forward steps ended at 27.8 ft (the two travel steps were missing).
  - The reverse steps ended at 36.0 / 32.9 / … / 22.7 instead of 20 / 16.9 / … / 6.9.
  - The lane matched the engine's grid exactly.
- **`PatternLoadScreens.Add`** (the game's hidden pattern-editor helper) returns an error string instead of adding when:
  - A zero-load step doesn't end *closer* than the step before it ("End footage must be smaller than start footage"). This holds in either direction, so **forward travel steps are always rejected**.
  - Plus other checks: at least 4 boards, "Loads in first forward screen can't be 0", "End of pattern reached", …
- **New-math `get_EndFootage`** for a reverse step with no previous step returns **`Pattern` field `0xC8`** (a template setting; 36 for template #0), whatever its own distance is. That pushed every reverse step 16 ft down the lane.
- **The game's own patterns don't go through `Add`.** They're built by the file parser.
- **1.4.2 draws custom patterns with BowlingPlus's own copy of `Graph3DForward` / `Graph3DReverse`** (`KegelDraw` in `Game.mm`), using each step's own end distance (the sheet's "End" column).
- **Checked:** compiled on its own and fed the steps the phone's engine got, it reproduced all 30 values of the device oil report exactly. On device, the oil report runs a self-check against the game's own drawing of the current pattern.
- **Reverse pass details:** it starts at row 0 with an empty board range (0..0).
  - Steps ending ≥ the brush drop keep the previous step's boards.
  - A zero-load step ending exactly at row 0 (travel to the foul line) **still lays `2 × old + 339 / speed`** over the last boards. On Baton Rouge that makes 0–7 ft, boards 5–35, heavy (82 units). The game does the same for its own patterns, so it's kept.

**The engine, fully decoded (1.4.1)** [files]
- **Grid:** `drawFromFile` allocates `Units = new float[41, 60]` (constant at `0x3E9F690`): boards × whole feet.
- **Rows:** a step covers rows `previous end row + 1 … Math.Round(EndFootage)` (round half to even).
- **Forward pass** (`Graph3DForward`), for each loaded step:
  - `Units[board, row] = 339 / speed` for boards start..stop.
  - Remembers each board's last oiled row, adds the loads to that board's total, and stores the speed per row.
  - **Boards outside the step**, and all boards during zero-load (travel) steps, accumulate machine time `rows / speed`.
- **Transfer brush:** after the forward pass, for boards 2–38:
  - `Units[b, end] = loads[b] × 1.5`
  - `slope = (Units[b, end] − Units[b, last]) / time[b]`
  - each row after the board's last oiled row adds `slope / speed[row]`.
  - So oil fades (or builds) from the board's last value to `loads × 1.5` at the pattern's end. This matches Kegel's "buff" zone.
- **Reverse pass** (`Graph3DReverse`), for each loaded step whose end row is **< `Pattern.mTravel` (`0xC4`)**: `Units[b, row] = 2 × old + 339 / speed`, from the current row down to the step's end.
  - So `mTravel` acts as the sheet's **Reverse Brush Drop**.
  - Custom patterns inherited the template's value until 1.4.1. Since then patterns carry `drop`; Baton Rouge = 34.
- **Python check:** a simulation of Baton Rouge with all of the above gives 24 (front, forward only), 56–67 (combined pyramid, 7–20 ft) and 9–24 (transfer zone, out to 39 ft).
- **Device mismatch (1.4.0):** the lane looked different (a staircase outline around 20–35 ft). Still open: the 1.4.1 "Copy debug info" oil report shows engine vs. lane values every 5 ft.

**How the engine turns steps into oil thickness** [files]
- **Forward pass:** `Graph3DForward` **sets** each covered cell (boards start..stop, rows across the step's distance) to **339 / speed**. 339.0 is the hex float `0x43A98000`. Loads only decide distance, not thickness, just like a real machine, where slower travel lays more oil per foot.
- **Reverse pass:** `Graph3DReverse` updates each covered cell as **new = 2 × old + 339 / speed**, so areas oiled by both passes come out much thicker (Kegel's "combined" area).
- **Zero-load steps** add no oil.
- **Baton Rouge example:** forward-only cells are about 19–24 units, combined cells about 56–67.
- **Oil per board:** built-in patterns use 50 µL (28 of them), 45 (3) and 40 (9), and the engine ignores it. So patterns from sheets are drawn exactly like the game draws its own.
- **The game's oil colors** (`OilColorData`, MaxHeight 100) use HSV-style keys at t = 0, 0.05, 0.06, 0.1 and 1. They go from red to blue within the first 6 units, then only slowly darken. So any oil thicker than about 10 units looks almost the same color on the lane. BowlingPlus previews use their own thickness ramp instead (0–75 units, light cyan → navy).

**How the lane shows oil** [files]
- **Shader** "Transparent/Diffuse Bowling Oil Recolored" (pathID 1114, Metal): `color = wood × mix(white, hsv(_OilHue, 1, 1), tex.G) × tex.R`.
- **Texture:** `GenerateRG16Texture` writes, per cell, `Gradient.Evaluate(oil / MaxHeight)` → `RGBToHSV` → **R = brightness (V), G = tint (S)**. Bare lane = white = no change.
- **The game's gradient** gives tint 0.4 for anything over ~6 units, then *less* tint and slightly darker up to 100. So thick and medium oil look alike.
- **Since 1.4.2, "Show oil thickness" is off by default** (the user preferred the original look; saved under a new key, `oilThick2`).
- **Oil color (1.4.2):**
  - The shader takes only a hue (`_OilHue`).
  - BowlingPlus sets it on the lane materials every frame when the user picks one.
  - "Use the game's color" calls the private `OilMapGenerator.UpdateOilColor()` to put the game's own hue back.
- **1.4.1 "Show oil thickness":**
  - Swaps the 5 `GradientColorKey`s (`Gradient.get/set_colorKeys`, 20-byte structs) and sets MaxHeight to 75.
  - Then redraws every cached picture with `GenerateRG16Texture(OilMatrix, ref tex)`.
  - Turning it off puts the originals back.

**Oil textures** [files]
- **The picture cache is keyed by list position** (`OIL_SELECTED − 1`), not `OilDescription.Id`. `ReloadCurrentOilMap()` = `ReloadOilMap(OIL_SELECTED − 1)`.
- **`GetTextureForId(index)`** (public static) only looks a picture up. It never draws one.
- **`ReloadOilMap(int index)`** (private) compares the cached copy with the pattern's live `OilMatrix`, redraws the picture if they differ, and finishes with `ReDrawOil`.
- **The carousel preview and the lane** both use `ShowOilReplay(index, line)`, which shows the cached picture.
- **Fixed in 1.4.0:** the custom-oil overlay now calls `ReloadOilMap` for the exact pattern it changes or restores. Before, a restore redrew the *current* pattern and left the old one's picture stale. 1.3.1's texture prep also used `OilDescription.Id` (wrong key), and is now fixed.
- **Shot replays:**
  - `ReplayData.ToXMLData` saves `OilToByteArray(index)` (the live `OilMatrix`).
  - `ReplayerInterfaceManager.switchReplay` calls `manageOilOnLane(recordId, ref oilTexture (0xA0), …)`, which builds a new texture with `GenerateRG16Texture` (Clamp). BowlingPlus sets it to Repeat every frame.
- `OilMapGenerator.GetTextureForId(int id)` (public static) returns a pattern's cached texture. `GenerateRG16Texture` only sets wrap Clamp when it **creates** a texture; redraws reuse it.
- [device] With BowlingPlus 1.3.0's flipped `_SizeX`, every newly created texture rendered blank until it was switched to Repeat, which showed as a blank flash when swiping the pattern carousel.
- Since 1.3.1 every pattern's texture is created once and set to Repeat in the background, and the lane's current texture is checked every frame.

**Practice data:** `Managers.PracticeManager._practiceData` (statics `0x20`) is an `mdl_Protocol_Practice_Data {tickets_left 0x10, oil_data mdl_Protocol_Oil_Data[] 0x18}`. Each entry is `{oil_id 0x10, state 0x14 (EOilState: DisabledLevel 0, LockedUseTickets 1, Unlocked 2), use_count 0x18, min_level 0x1C}`.

**Applying a practice pattern:** `RunPsycsTest.applySetting` writes `OIL_SELECTED` and calls `OilMapGenerator.ReDrawOil()`. The practice lobby's `PracticeOil.HandleApplyClicked` uses `GameParams.SetSetting`, then `applySetting`, then `PlayerContext.set_PracticeOil`.

## 11a. Android port (APK 1.907, build 1597) [files + device]

Everything here was read from the game's own Play bundle (`base.apk` + `split_config.arm64_v8a.apk`) unless marked [device].

| Fact | Value | Source |
|---|---|---|
| Package / version | `studio.wannaplay.bowlingjb`, versionCode 1597, versionName 1.907, `requiredSplitTypes="base__abi"` (only the native libraries are split off) | [files] manifest |
| Launcher activity | `com.google.firebase.MessagingUnityPlayerActivity`, a subclass of `UnityPlayerActivity` (which holds the protected field `mUnityPlayer`) | [files] manifest + dex |
| `libmain.so` | Exports only `JNI_OnLoad`, so BowlingPlus's `libmain.so` can take its place and forward to the original, renamed `libmain_orig.so` | [files] `readelf` |
| `libil2cpp.so` | Exports every `il2cpp_*` function the port calls | [files] `readelf` |
| DNS patch | The `getaddrinfo` import slot sits in a RELRO page (read-only after load, BIND_NOW), so patching it needs `mprotect` | [files] `readelf` |
| Dex files | 6 (`classes.dex` to `classes6.dex`); the Java side is added as `classes7.dex` | [files] |
| Unity main thread | `UnityPlayer.invokeOnMainThread(Runnable)` is public; it adds to a queue that only `executeMainThreadJobs` empties, and `libunity.so` calls that, so the job runs on Unity's own thread | [files] dex + `libunity.so` |
| Facebook | `LoginManager.setLoginBehavior`, `LoginBehavior.WEB_ONLY`, `AccessToken` / `Profile` getters used by `FbLogin.java` all exist; `CustomTabActivity` is registered for `fbconnect://cct.studio.wannaplay.bowlingjb` | [files] dex + manifest |
| ZXing | Every class and method `Qr.java` uses exists (`QRCodeWriter`, `MultiFormatReader`, `RGBLuminanceSource`, `HybridBinarizer`, `BitMatrix`) | [files] dex |
| Privacy page | `assets/MRGSGdpr/WPSmrgsgdpr.html` contains "By clicking Sign up"; its `clickButton()` does `document.forms["form"].submit()` | [files] |
| Tamper checks | No `com.pairip` / license client. The Play Integrity client library is present, and `getPackageInfo` / `signingInfo` strings appear in the dex; whether the game or its servers use them is not known | [files] |
| `patch_apk.py` dry run | On this bundle: splits merged, `libmain_orig.so` + new `libmain.so` + `classes7.dex` added, split markers and the Play stamp removed, picker relay and file provider added, manifest reads back identical. Signing not run (no build-tools available) | [files] |

Android behavior seen on a phone [device, 1.6.0 build]:

- **Worked:** the on-screen menu button; importing a Kegel PDF (no crash).
- **Reported and fixed in 1.6.1:** the shake menu scrolled at roughly 10-15 FPS; the Custom oil card sat against the left edge (cards that give only a vertical gravity get no horizontal centering); the footer's first line was left-aligned; the on-screen button drew over the Custom oil title.
- **Why the menu was slow (a likely cause, not measured):** with 120 FPS mode on, the lane behind the menu was still drawn at 120 FPS, sharing the GPU with the menu's scrolling. 1.6.1 caps `Application.targetFrameRate` to 30 while a panel is open. If it is still slow after this, the next suspects are the dim full-screen overlay over the Unity surface and the layout cost of the menu itself.

Not verified on Android yet: signing and installing the patched APK; whether Facebook accepts the browser login on a re-signed build (Copy log shows an `[fb]` line with the answer); whether the game's servers reject a re-signed build (Play Integrity); the 1.6.1 frame-rate cap on a device.

## 11b. The game's two-lane system [files: IL2CPP metadata v31, 1.907]

Read from `global-metadata.dat` (record sizes: type 88 bytes, method 36, field 12; checked by finding `RunPsycsTest._kegsUp`, `kegsUpdate` and `UpdatePinPositions`). Names only: field types, values and method bodies are not in the metadata.

- **`RoadChanger`** (MonoBehaviour): fields `objectsToShift currentShift currentLane shiftBy toActivate0 toActivate1 test oil bumperManager oneLane laneNum`; methods `getCurrentLaneGame()`, `getCurrentLaneSaved()`, `shiftToLaneNumber(1 arg)`.
- **`SwitchLaneDrag`** (MonoBehaviour, the drag-your-shoes gesture): fields `ballDrag test dragging _intialPosition _initialDragPosition`; methods `OnPointerDown`, `OnDrag`, `OnEndDrag`.
- **`DragManager`**: `firstLaneLeft`, `secondLaneRight` among its fields (the aiming drag).
- **`SettingType`** enum: `G_SOUNDS OFF_CROSSOVER CHANGE_OIL CHANGE_LANE SHOW_OIL_PATTERN LIFT_BUMPERS FRAME_COUNT CORE_STATE VIBRATION G_SHOW_TRAJECTORY G_SHOW_BALLINFO G_SCORE_TYPE G_LEFT_HAND`, read through `GameSettings.GetSetting / SetSetting`. `Bowling.GUI.PracticeSettings` only has trajectory, projector, bumpers and restart, so nothing on screen sets `CHANGE_LANE`.
- **Per lane:** `LightIndication` (`lightsOnLane1/2`, `lightsOffLane1/2`), `PositionSave` (`laneID`, `oil_id`, `playerID`), `ReplayData.LaneId`, `ReplayerInterfaceManager.changeToAppropriateLane()`. The oil generator's `Lines` list has 2 lane renderers (`lanes=2` in debug info).
- **[device, 1.6.2, Practice at the start position]** `RoadChanger` x1: `laneNum=0`, `oneLane=0`, `shiftBy` is a `System.Single[]` (set), `currentShift=-1.837`; `currentLane` printed 475208576 = `0x1C531B80`, the low half of the object's class pointer, so it is almost certainly a **static** field (offset below `0x10`) and 1.6.3 reads it as one. `SwitchLaneDrag` x1 **active** in the scene. `DragManager.firstLaneLeft = -0.700`, `secondLaneRight = 8.500`.
- **[device, 1.6.3]** `currentLane` read as a static: **1**. `shiftBy = [0.000, -1.837]` (2 lanes), `currentShift = -1.837` (lane 1), `shiftToLaneNumber(int)` = yes. **Dragging the shoes to the other lane works, but they snap back to the left lane on release.**
- **1.6.4** added an experimental switch that called `shiftToLaneNumber(other)` at the ball rack. **[device, 1.6.4] It gave up: moved back 5 of 5 times** (`home=1 now=1 tries=4 reverts=5`). A screenshot shows the right-hand lane in Practice with **no pins, no oil and no lane lights**: it isn't set up, so moving the player alone can't work. Removed from the menu in 1.6.5 (code kept, off).
- **[files, 1.6.6] The other lane is switched off in the game's own code.** `RoadChanger.shiftToLaneNumber(int i)` (Android `0x17AFCC0`, iOS `0x1EDB6F4`) **never reads `i`** (both binaries: its argument register is overwritten before any read). It computes `target = laneNum > 1 ? laneNum : 1` (`laneNum` `0x64` = 0 here, so **lane 1**); if the static `currentLane` already equals it, it returns. Otherwise: `currentLane = target`, `currentShift` (`0x28`) = `shiftBy[target]`, every `objectsToShift` moves by the difference, lane 0 activates `toActivate0` and deactivates `toActivate1` (lane 1 the reverse), then `RunPsycsTest.ManageLightsOverPindeck`, `OilMapGenerator.ShowOilOnLane` (if active), dragger/camera resets by location, and `applySetting(GetSetting(...))`. So the game can never move you to lane 0, and lane 0's own objects (`toActivate0`) stay off: that is the empty lane in the 1.6.4 screenshot. 1.6.4's `shiftToLaneNumber(other)` calls were no-ops (already on lane 1), not "moved back".
- `getCurrentLaneGame()` returns the static `currentLane` (no callers). `getCurrentLaneSaved()` returns `GameParams.GetSetting(3)` (`CHANGE_LANE`). Callers of `shiftToLaneNumber`: `RunPsycsTest.applySetting`, `InfoScreenManager.changeToOurTurn` / `UpdateSelectedLane`, `RunPsycsTest.playMultiplayerReplay` / `PlayDemoReplay` / `switchToTutorial` / `HideReplayInterface`, `ReplayerInterfaceManager.changeToAppropriateLane`: each would move a player on lane 0 straight back to lane 1.
- **Next step for a playable second lane:** do `shiftToLaneNumber`'s work for lane 0 from BowlingPlus (the steps above), and redo it after each of those callers. Not read yet: `SwitchLaneDrag.OnEndDrag`, what `toActivate0/1` contain, and whether the oil generator's two `Lines` keep separate patterns.
- **Not known yet:** whether a lane moved by `shiftToLaneNumber` stays, and whether the two lanes keep separate oil.

**Startup** [device, 1.6.1]: BowlingPlus found the lane controller at 6.40 s, the core loop was in `MainMenuState` at the same moment, Practice was entered at 10.14 s, and the 4 s settle wait put the oil and ball fixes at 10.42-10.59 s. 1.6.2 settled 1.5 s after `MainMenuState` [device: better, still beatable]; 1.6.3 settles the moment the core loop is in `MainMenuState` (exact class name; any other state keeps the 4 s wait).

## 12. Not verified yet

- **Seen working on device (1.3.0):** the custom oil library and lobby tab, a saved custom pattern with its preview, and the oil drawn on the lane in the practice pattern screen.
- **[device] iOS pop-up menus** (`UIButton.menu` with `showsMenuAsPrimaryAction`) don't open on BowlingPlus's overlay inside the game, even with the row's tap handler skipping the button. Since 1.4.0 every choice uses BowlingPlus's own panel. Share sheet and photo picker open from a small window of BowlingPlus's own.
- **[device, 1.4.2] Menu layout:**
  - **Arsenal search:** the text field had no lower compression resistance than the buttons, so typing squeezed **Clear** to nothing.
  - **Header ✕:** the title and ✕ both had default hugging, so the ✕ got stretched with its icon centered.
  - Both fixed in 1.4.3.
- **[device, 1.4.0] Replays showed the replaced pattern:** `gBFStatus.offline` is false while `LOC_REPLAYER` is up, so the custom/invisible oil restored the original for the replay. Fixed in 1.4.1 (`InPracticeOil` counts the replay as practice).
- **[device, 1.3.1] Bugs reported and fixed in 1.4.0:**
  - The custom pattern differed depending on which pattern it replaced (it was built from that pattern's file as the template).
  - Custom oil stayed on a pattern's carousel preview after being turned off.
  - Replays didn't show the oil.
- **New in 1.3.0/1.3.1 and not yet confirmed on a device:** the mirror fix during play, show breakdown, invisible oil, QR import, and the 1.3.1 carousel flash fix.
- **Confirmed fixed on device (user, after 1.2.3):** networking with NextDNS (IPv4 filter + watchdogs) and ball/pin physics.
- **New in 1.2.0/1.2.1 and not yet tested on a device:**
  - 120 FPS mode.
  - The tutorial-stage cleanup after a skip.
  - Privacy-popup auto-accept.
  - The loading watchdog: whether it rescues the NextDNS hang, and which wait actually hangs. It did not cover the "disconnected from the server" screen [device, 1.2.1].
  - The 1.2.3 IPv4 DNS filter: whether it ends the NextDNS hangs. The 1.2.2 log showed the cause, but the fix hasn't been tested.
  - The 1.2.3 spinner watchdog.
  - Pins-only speculative collisions.
- **iOS on-screen menu button and Back up my data (added after 1.6.0):** not seen on an iPhone. They did not compile until 1.6.1 (helpers used before their declaration in `MenuButton.mm`). Whether the app delegate's `window` is the game's Unity window on this build is assumed from `Privacy.mm`'s use of it, not checked separately.
- **Pins clipped low that "barely budge":** still open. See section 5.
- **Why the MRGS "Sign up" page repeats in a sideloaded copy:** not proven.
- **Runtime skin downloads:** whether the game downloads extra skin packs at runtime (`packs` bundle, `ResourceMeta.mdt`) hasn't been looked into.
