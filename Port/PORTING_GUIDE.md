# BowlingPlus: implementing these features natively in the official game

For: the Bowling by Jason Belmonte team, who have the Unity project and source.
Covers: BowlingPlus 1.7.5. Engine: Unity 6000.0.67f1, IL2CPP (the version the shipped game uses).

BowlingPlus adds these features to the finished app at runtime, by changing values in memory. That was the only option without the source. You have the source, so each feature below is written as a change to your project: a setting, a prefab, a material, a Project Setting, or a line of C#. Appendix A describes how BowlingPlus does it at runtime. You can skip it.

Items marked **(confirm)** are things we haven't verified in your project or in Unity's current docs. Check them before relying on them.

## 1. How to read this

- **Effort** is our estimate for your code: 1 = hours, 2 = a day or two, 3 = several days, 4 = a week or more with testing.
- **Gate** is where the feature should be allowed:
  - **Any**: visual, bug fix, or setting that doesn't change scores. Safe in every mode.
  - **Practice**: only in offline Practice. Changes conditions or outcomes.
  - **Decision**: changes scores or physics for everyone, so the team decides per mode.
- **ID** is the name used in section 5, if BowlingPlus keeps running alongside your build.

## 2. Summary, easiest first

| # | ID | Feature | Effort | Gate | Where in your project |
|---|----|---------|--------|------|-----------------------|
| 1 | `bp.fps120` | 120 FPS mode | 1 | Any | Frame-rate setting, iOS build plist, Android activity |
| 2 | `bp.ipv4` | IPv6 loading fix and loading time limits | 1-2 | Any | Networking config or server DNS; loading screen timers |
| 3 | `bp.banner` | Sweeper "BELMO" banner | 1 | Any | Sweeper prefab (canvas and draw order) |
| 4 | `bp.pinTurns` | Pin turns kept through a re-rack | 2 | Any | Pin spawn and rack code |
| 5 | `bp.bg` | Alley background image | 2 | Any | Background UI and image import |
| 6 | `bp.pinLib` | Pin texture library with 3D preview | 2 | Any | Pin material and a preview camera |
| 7 | `bp.backup` | Backup and restore of settings and patterns | 2 | Any | Your save system |
| 8 | `bp.noTap` | 9-pin no-tap mode | 2 | Practice | Scoring rules for the mode |
| 9 | `bp.invisOil` | Invisible oil | 2 | Practice | Oil display setting and oil redraw |
| 10 | `bp.oilLib` | Custom oil patterns | 3 | Practice | Oil pattern data (`OilDescriptionData`) |
| 11 | `bp.oilBreak` | Oil breakdown across shots | 3 | Practice | Oil map generation per shot |
| 12 | `bp.spare` | Spare mode and pin picker | 3 | Practice | Pinsetter setup and a pin-picking UI |
| 13 | `bp.speed` | Ball speed and spin tools | 3 | Practice | Throw code, at launch |
| 14 | `bp.replayRate` | Replays at the recorded rate | 1 | Any | Replay recording and playback |
| 15 | `bp.pinFric` | Pin friction (0.40, or 0.25 with item 16) | 2 | Decision | Pin physics materials |
| 16 | `bp.doubleRate` | Double physics rate (3.75 ms step) | 3 | Decision | Project Settings > Time, plus per-step code |

Startup and connection (details in `features/startup-and-connection.md`):

| # | ID | Feature | Effort | Gate | Where in your project |
|---|----|---------|--------|------|-----------------------|
| 17 | `bp.errReporter` | Error reporter that never delivers: fix its URL and host, or remove it | 1 | Any | `CloudReporter`, created in `InitState.OnEnter` |
| 18 | `bp.startupSdk` | Start the SDKs together, with time limits | 2 | Any | `InitState.InitAfterSceneLoaded` |
| 19 | `bp.connectEarly` | Connect during the loading bar | 3 | Any | `Connector`, `ClientSideManager`, `LoadingWindow` |
| 20 | `bp.splitScene` | Split the `Game3` scene (removes the 84 % stall) | 4 | Any | Scene setup, additive scenes or Addressables |

New in 1.7.5:

| # | ID | Feature | Effort | Gate | Where in your project |
|---|----|---------|--------|------|-----------------------|
| 21 | `bp.hideStats` | Hide the ball stats (in hand and on the rack) | 1 | Any | `Ball_Description` and `BallOnReturnerInfo` prefabs, one setting |
| 22 | `bp.arsSort` | Arsenal order (name or stat, either way) | 1 | Any | `ArsenalBallManager.InitScrollData` |
| 23 | `bp.modePicker` | Game mode button that restarts at frame 1 | 1-2 | Practice | The practice pattern window (`PracticeOil`) |
| 24 | `bp.loft` | Loft slider | 2 | Practice | Throw code at launch, ball landing |
| 25 | `bp.ramps` | Ramps & obstacles mode, with a best score | 3-4 | Practice | A new Practice mode: prefabs, layouts, a scoring rule |

## 3. Native implementation, feature by feature

### 1. 120 FPS mode (`bp.fps120`)

**What it does:** runs menus and gameplay at 120 FPS on 120 Hz phones. The game draws menus at 30 and gameplay at 60 on phones.

**Native implementation:**
- Read the setting and set `Application.targetFrameRate` from one place: the code that already sets it in `Loading.Start`, `RunPsycsTest.StartFun`, `RunPsycsTest.restartGameComplite`, `FloatingMenu` and `HTHLobbyManager`. Keep `QualitySettings.vSyncCount = 0`.
- Set the menu and game rates from the setting, not from fixed constants. The stock values are `MENU_FRAME_RATE` = 30 and `GAME_FRAME_RATE` = 60 in `Client.Core.Constants`.
- **iOS:** the stock `Info.plist` has `CADisableMinimumFrameDurationOnPhone` set to `false`, so iOS holds the app at 60 Hz whatever the game asks for. Set it to `true` in your iOS build. **(confirm)** The usual way is a post-build step that edits the plist (`PostProcessBuild` with `PlistDocument`). Check that it's in the built app, not only in your source.
- **Android:** Unity doesn't expose a refresh-rate request. Ask for the display's fastest mode with `Window.getAttributes().preferredDisplayModeId` from a small Android plugin, choosing the mode with the same resolution and the highest refresh rate. **(confirm)** Check the current API for Android 6.0+ and Unity 6's plugin setup.
- If an overlay scrolls over the lane, cap the lane's rate while it's open. Drawing the lane at 120 FPS behind a scrolling menu was slow.

**Watch out:** the physics runs on a fixed timestep, so the render rate shouldn't change shots. Confirm with a test: the same shot at 60 and 120 FPS should match.

**Test:** Unity Profiler frame times on a 120 Hz iPhone and an Android phone. Check that the setting goes back to 60 when turned off.

### 2. IPv6 loading fix and loading time limits (`bp.ipv4`)

**What's wrong:** the game's networking library (Photon, in the build we analyzed) picks the first IPv6 address it gets for the game's servers. The servers didn't answer on IPv6 in our tests, so with DNS that returns IPv6 (NextDNS does) every connection hangs and retries forever.

**Native implementation, in order of preference:**
1. **Server side:** make the game's servers answer on IPv6, or stop publishing IPv6 records for them. No client change.
2. **Client, one rule:** in the networking library's address selection, prefer an IPv4 address when one exists for the game's hosts. Check the version you ship; the selection code is in the library, not your game code.
3. **Loading time limits (do these regardless):** in your loading screen code, add timers:
   - Stuck on "connecting" for 30 seconds: show the offline section (the game's own offline button).
   - Privacy SDK waiting for 20 seconds with no privacy page on screen: treat the wait as done, the same as having no internet.
   - Waiting spinner with no server answer for 30 seconds: hide it so the player can try again.
   Use a coroutine (`WaitForSeconds`) or a timer, and reset it whenever the state changes.

**Watch out:** a time limit can hide a real connection that's just slow. Make it a short message, not a silent skip, and let the player retry.

**Test:** run with a DNS that returns IPv6 for the game's hosts. Without the fix, loading never ends. With option 1 or 2, it connects. With option 3 only, the offline button appears after about 30 seconds.

### 3. Sweeper "BELMO" banner (`bp.banner`)

**What's wrong:** the banner is a world-space canvas, recessed inside the sweeper bar. The bar draws over both ends, so the B and O are cut off. Near the back of the lane the banner drops out for a few frames and looks black.

**Native implementation:** in the sweeper prefab, bring the banner's canvas in front of the bar (sorting order, or the bar's render queue), or move the banner forward so it's not inside the bar. Check that the banner's width covers the whole bar, and that the canvas isn't clipped by the sweeper's animated bones.

**Test:** a sweep to the back of the lane on the device, with the banner in view the whole way.

### 4. Pin turns kept through a re-rack (`bp.pinTurns`)

**What's wrong:** the game gives each pin a new random turn on every rack. Picking up a ball re-racks, so pins visibly turn.

**Native implementation:** in your pin spawn code (`InventaryData.ResetRigidBody` is where the game rebuilds each pin's Rigidbody at every rack), choose each pin's turn once, when it's spawned for the match, and reuse it on every re-rack. Don't re-randomise the rotation in the rebuild.

**Test:** pick up a ball several times; pin rotation should stay the same between racks.

### 5. Alley background image (`bp.bg`)

**What it does:** the player chooses their own picture behind the lanes, with Fill, Fit, Stretch or Tile.

**Native implementation:** a background `RawImage` (or an `Image` with preserve aspect) showing the chosen picture. Fill: scale to cover and crop. Fit: scale to fit, with a blurred copy behind so there are no black bars. Stretch: set to the screen. Tile: set `RawImage.uvRect` to repeat. Import pictures from the gallery and files with your usual plugin, and store the copy in `Application.persistentDataPath`.

**Test:** a tall, a wide and a square picture in each mode.

### 6. Pin texture library with 3D preview (`bp.pinLib`)

**What it does:** the player picks a texture for the pins, with a turning 3D preview before confirming.

**Native implementation:** swap the texture on the pin material (the main texture slot) for the chosen picture. For the preview, render the pin mesh with the same material into a `RenderTexture` from a separate camera, shown in the library UI. Store the pictures in `persistentDataPath` and the choice in your settings.

**Watch out:** a replay should keep the texture it was recorded with. Store the texture choice with the replay, or leave the texture off replays.

**Test:** the preview and the lane should look the same.

### 7. Backup and restore of settings and patterns (`bp.backup`)

**Native implementation:** this depends on your save system. Export the settings and any custom patterns to one file, and import it back with a version check. Nothing here is specific to the game's engine.

### 8. 9-pin no-tap mode (`bp.noTap`)

**What it does:** on a full rack, 9 or more pins knocked down on the first ball counts as a strike, and the frame ends.

**Native implementation:** this is a scoring rule, not a physics change. In the code that decides the frame result (the code that counts pins once they've settled; the game waits 1.5 seconds after they stop, using a timeout), add the mode's rule: if the first ball leaves 9 or fewer standing and 9 or more are down, score a strike. Keep it in the scoring code for that mode only.

**Watch out:** the game's own pin count must stay correct. If you change the count, check the score sheet and the replay.

**Gate:** Practice only, since it changes scores.

**Test:** a full rack with 9 down on the first ball; check the score sheet and the next frame.

### 9. Invisible oil (`bp.invisOil`)

**What it does:** a mode where the oil isn't shown. A random unlocked game pattern is used each game.

**Native implementation:** in the oil display code (`OilMapGenerator`, where the "show oil" setting is checked before drawing), set the display off while the mode is on. Set it in memory only, so it isn't saved into the player's settings. Turning the mode off restores the saved value.

**Watch out:** the oil flashed on the lane for a moment after a ball was picked, because the ball selection redraws the oil. Make sure every redraw checks the same setting.

**Gate:** Practice only.

**Test:** pick a ball several times with the mode on; no oil should appear, even for a frame.

### 10. Custom oil patterns (`bp.oilLib`)

**What it does:** players add their own patterns from Kegel data sheets, a QR code or a text code, and share them.

**Native implementation:** patterns are `OilDescriptionData.OilDescription` entries in your data. Add an import path that creates those entries from the same numbers (the data sheet's board distances, oil volumes and steps). Keep custom patterns separate from the official list, so official data is never overwritten.

**Gate:** Practice only, since the pattern decides the difficulty.

**Real-life drawing of the built-in patterns:** BowlingPlus can also redraw the game's 48 built-in patterns in Practice from their own Kegel files (exact distances, microliters, the brushed film). Because online play uses the game's simpler drawing, players practicing for online matches need it switchable; BowlingPlus 1.7.4 has it as a switch, on by default. If you port it, make it a Practice option and keep online on one drawing.

**Test:** import a known data sheet and check the oil is laid out as printed.

### 11. Oil breakdown across shots (`bp.oilBreak`)

**What it does:** the oil changes from shot to shot, as it does on a real lane, instead of resetting.

**Native implementation:** in the per-shot code, regenerate the oil map from the current state after each shot. Gate it on Practice.

**Test:** several shots in a row; the oil should change in the expected direction.

### 12. Spare mode and pin picker (`bp.spare`)

**What it does:** the player chooses which pins stand for a spare shot.

**Native implementation:** a Practice-only setup UI that sets which pins are placed in the rack, using the same spawn code as a normal rack.

**Gate:** Practice only.

**Test:** set a 7-10, a 3-6-10 and a single pin; check the rack.

### 13. Ball speed and spin tools (`bp.speed`)

**What it does:** scales the ball's speed once, at launch, for practice.

**Native implementation:** in your throw code, when the ball leaves the throw state, multiply the ball's `Rigidbody.linearVelocity` by the chosen factor, once. Keep the factor in Practice only. Above about 5x it felt too strong in our testing.

**Test:** the same throw at 1x, 2x and 3x.

### 14. Replays at the recorded rate (`bp.replayRate`)

**What it does:** a replay plays at the same speed it was recorded at.

**Native implementation:** the replay records one frame per physics step and plays one frame per step. If you change the fixed timestep (item 16), change it once, so recording and playback use the same step. Check any playback code that assumes a fixed frame rate.

**Test:** record a throw and play it back; the replay should last as long as the throw.

### 15. Pin friction (`bp.pinFric`)

**Read first:** BowlingPlus 1.7.0-1.7.4 and the first version of the pin study modelled the game's materials with the combine mode **Minimum**. Your materials store 1, which is **Multiply** (Unity's enum: Average 0, Multiply 1, Minimum 2, Maximum 3). Rerun with Multiply, the game as shipped already strikes about as often as real pins on Bowlscore. If you read an earlier copy of this folder, its pin numbers (25 %, 37 %) are wrong: see `pin-physics/results/CORRECTION.md`.

**What it does:** sets the pins' friction (the `Pin` material on the colliders, and `PinButtom` on the base). Because the materials multiply, a pin material at 0.40 gives pin on pin 0.16, pin on deck 0.24 sliding, pin on ball 0.12.

**Evidence** (PhysX 4.1 model of the game's pins, `pin-physics/`, 1,012 Bowlscore shots each, ±3 points; real pins 41.9-44.3 %):

| Setup | Bowlscore strikes |
|---|---|
| The game as shipped (0.5 sliding / 0.3 starting, 7.5 ms step) | 46.5 % |
| Pin material friction 0.40, 7.5 ms step | 44.0 % |
| Pin material friction 0.25, 7.5 ms step (BowlingPlus 1.7.0-1.7.4) | 38.4 % |
| 3.75 ms step (item 16), stock friction | 50.0 % |
| 3.75 ms step, pin material friction 0.25 | 44.0 % |

**Native implementation:** if you change nothing else, the stock pins are already close (a little above the real range). To land inside it: set the `Pin` and `PinButtom` materials' static and dynamic friction to **0.40** in the asset; or, if you adopt the 3.75 ms step (item 16), to **0.25**. Keep the combine mode at Multiply (the numbers above assume it).

**Gate:** Decision. It changes scores for everyone, so decide per mode. A safe path: Practice first, then compare leaderboards before any wider change.

**Test:** the Bowlscore runs in `pin-physics/results/`, and the same shots on a device.

### 16. Double physics rate (`bp.doubleRate`)

**What it does:** halves the fixed timestep, from 7.5 ms to 3.75 ms, so pins are simulated in smaller steps.

**Native implementation:** set the fixed timestep in Project Settings > Time (or the time settings asset) to 0.00375 seconds. Then review everything that runs per physics step:
- oil transfer (it's computed per step; check that it depends on distance, not step count),
- friction and any per-step counters,
- replay recording and playback (item 14),
- anything that assumes 7.5 ms.

**Evidence:** in the model, halving the step roughly halves the numerical error of every hit (the ball sinks 10 mm into a pin before the hit is resolved instead of 25 mm; pin-on-pin outcomes that differ from a converged run drop from 22 to 11 of 360). It also raises Bowlscore strikes from 46.5 % to 50.0 %, so pair it with pin friction 0.25 (item 15: 44.0 %). The step change was tested only in that model.

**Gate:** Decision. As with item 15, and it also changes replays and oil behaviour, so test those first.

**Test:** the review above, then play on a device.

### 21. Hide the ball stats (`bp.hideStats`)

**What it does:** a setting that hides the stat panel on the ball in the player's hand and on the balls in the ball rack.

**Native implementation:** the in-hand stats are all under the child `Hook` of `BallManager/Canvas` (`Ball_Description`); the rack's are the child `OBJ_arc_all_120x90` of each `ObjectsToShift/ObjectsOnBallSelect/ball1..ball10` (`BallOnReturnerInfo`). Gate those two objects' `SetActive` on the setting where the scripts already decide to show them (`Ball_Description.enableCanvas` / `CanShowBallInfo`, `BallOnReturnerInfo.Update`), and leave the rack's select button (`butt`) alone. Setting 10 (`G_SHOW_BALLINFO`) is the ball's shadow, not these.

**Gate:** Any (display only).

**Test:** pick a ball and open the overhead rack view with the setting on and off.

### 22. Arsenal order (`bp.arsSort`)

**What it does:** the Arsenal list is sorted the player's way every time it opens: by name (A-Z, Z-A) or by power, hook, length, RPM or condition (high to low, low to high).

**Native implementation:** `ArsenalBallManager.InitScrollData` builds `_ballsData` and sorts it with `BallComparer`. Choose the comparer from the setting: name from the item's display name, power as `power + extraPower`, hook `hook`, length `lenght`, RPM `RPM`, condition `conditionCurrent` (fields of `mdl_Item_Ball`). Use a stable sort with your current order as the tie-break.

**Gate:** Any (list order only).

**Test:** each order, with and without a search filter, and after buying or drilling a ball.

### 23. Game mode button with a restart (`bp.modePicker`)

**What it does:** a button on the practice pattern screen (under the pattern list) picks a mode: Normal, Invisible oil (item 9), 9-pin no-tap (item 8) or Ramps & obstacles (item 25). Picking one starts the game again at frame 1.

**Native implementation:** add the button to the `PracticeOil` window and restart the way its own Apply does (`HandlePlayPracticeUpdated`): the session's `AddMode = 1`, `RunPsycsTest.restartGame(true)`, `RunPsycsTest.StartFun(true, true, false)`, then `FloatingMenu.closeClick()`. Store the mode in Practice's settings.

**Gate:** Practice.

**Test:** pick each mode in the middle of a game; the score sheet should start empty at frame 1 with the mode on.

### 24. Loft (`bp.loft`)

**What it does:** a vertical LOFT slider on the left of the screen while the player holds the ball: 0 (the normal release) to 16 ft, in half feet, kept until changed. The ball is thrown out over the lane and comes down that far past the release.

**Native implementation:** at launch (where item 13 scales the speed), add an upward speed so a flight of `d` metres at forward speed `v` lands at `d`: `vz = g d / (2 v)`. The lane bounces (0.65 x 0.65 = 0.42 with Multiply), so a ball dropping from the air hops; BowlingPlus removes the ball's upward speed when it touches the lane again after flying (before the pins only). Natively you could give the lane contact zero bounce for the ball while it's in loft flight instead.

**Gate:** Practice (it changes shots).

**Test:** 0, 4, 8 and 16 ft at slow and fast speeds; the ball should land where the slider says and not hop.

### 25. Ramps & obstacles (`bp.ramps`)

**What it does:** a Practice mode. Every new rack gets a random lane layout from 42: 28 with ramps (1 to 3 ramps, 0.26-1.0 m wide, 8-32 cm high, 8-22 degrees, 1.9-11.5 m from the foul line) and 14 with barstools (1 to 4). Ramps and barstools are never on the lane together. The ball has to roll over a ramp, and mustn't touch a barstool, or the shot scores 0. In the 10th frame a ramp has a 45 % chance of sliding slowly across the lane. The best game played in the mode from frame 1 is kept and shown on the mode button's screen.

**Native implementation:**
- Make ramp and barstool prefabs in your art style. A ramp's top must start flush with the lane (BowlingPlus sinks its front edge 2 mm) so the ball rolls on without a bump; its collider needs no bounce (a material with bounciness 0; Unity's default material meets the ball's Multiply as 0.18 friction, no bounce).
- The layouts are in BowlingPlus's `src/RampLayouts.h` (generated by `tools/dev/ramps/make_ramps.py`, which also checks them). Every ramp was simulated in the PhysX model at 5.5 to 10 m/s, aimed at its centre and 5 cm inside each edge: the ball climbs it, stays on the lane and is rolling again by 17.6 m. Every row of stools leaves a gap of 27 cm or more.
- Scoring: decide per shot whether the ball climbed a ramp (or touched a stool) and, if not (or if so), score 0. BowlingPlus does it by moving the ball into the gutter (just above the gutter's middle, so it isn't pushed out of a collider), which your scoring already counts as 0; natively you'd set the throw's result directly.
- Landing: the same damping as item 24.
- The best score: `Game.GetGameScore()` at `endOfGame`, only for games played in the mode from frame 1.

**Gate:** Practice.

**Test:** every layout once (BowlingPlus logs each one by name), slow and fast balls, a miss on purpose, a stool hit, and a 10th frame.

### 17-20. Startup and connection

See `features/startup-and-connection.md`. In short: the loading bar's 84 % stall is Unity switching on the whole game scene (`Game3`, 7,310 scripts), and the server connection only starts after that, because the `Connector` lives in `Game3` and is woken in `MainMenuState.OnEnter`. Start connecting during loading, split `Game3`, start the SDKs together with time limits, and fix or remove the error reporter, which has never delivered a report (its URLs are malformed and its host doesn't exist).

## 4. Practice gating in your code

BowlingPlus only changes gameplay in offline Practice. In your code:

1. Add one check, `PracticeFeatures.IsAllowed(gameMode)`, that's true only for Practice.
2. Gameplay features read their value through that check. Competitive, online, league and tournament modes always see stock values.
3. Keep cosmetic features and bug fixes (items 1 to 7 and 14) outside the check.
4. Mark replays with the settings they were recorded under, or don't allow practice-modified shots into shareable replays.
5. Items 15 and 16 are Decision items. Decide them per mode.
6. Items 23 to 25 change outcomes, so they're Practice only; 21 and 22 are display only.

BowlingPlus's own check is `gBFStatus.offline = (mode == Practice) && (not the replayer)`. The idea is the same. Use your own enum.

## 5. If BowlingPlus keeps running alongside your build

If BowlingPlus keeps shipping with the official game, it can apply a fix twice. The table shows what happens and what to do:

| ID | If the official build has it | What to do |
|----|------------------------------|------------|
| `bp.fps120` | The same setting exists | Hide BowlingPlus's switch. Its code writes the same values, so running both is harmless. |
| `bp.ipv4` | Your client or server fix is in | Hide BowlingPlus's switches. The DNS filter does nothing once lookups return IPv4. |
| `bp.pinFric` | You've set the pin materials' friction | Hide the switch: BowlingPlus would set its own value on top of yours. |
| `bp.hideStats` | Your setting exists | Hide BowlingPlus's switch. Both only switch the same objects off. |
| `bp.arsSort` | Your sort exists | Hide BowlingPlus's order setting, or it re-sorts your list. |
| `bp.modePicker`, `bp.ramps` | Your mode picker or mode exists | Hide BowlingPlus's button and mode. Its restart uses the game's own calls. |
| `bp.loft` | Your loft exists | Hide BowlingPlus's slider, or a throw gets both. |
| `bp.doubleRate` | **Known gap:** BowlingPlus halves whatever step it finds in the 2 to 50 ms range, not only 7.5 ms. | Don't run both until BowlingPlus checks for exactly 7.5 ms. |

Tell us which IDs you ship, and when. We'd then stand BowlingPlus down on builds that already have a feature.

## 6. Testing status, honestly

- **120 FPS:** the iOS flag was confirmed on a phone (BowlingPlus 1.6.1). Smoothness has only been reported by users. Android compiles and passes CI; no phone test is recorded.
- **IPv6 fix:** confirmed on an iPhone with NextDNS after BowlingPlus 1.2.3. The Android port compiles and passes CI; no phone test is recorded.
- **Pin physics:** the percentages come from our PhysX 4.1 model of the game, not from the game itself, and were corrected in 1.7.5 (the materials' combine mode; `pin-physics/results/CORRECTION.md`). A few phone games on the double rate were played; that's too few to measure a strike rate.
- **1.7.5 (items 21-25 and the new pin friction):** not yet tested on a phone. Both platforms compile, the ramp layouts pass the PhysX model, and the game's calls they use exist in both builds' engines.
- **Startup:** the sequence was read from the Android build; the timings and log findings come from one iPhone 17 Pro log.
- **Everything else:** compiled and checked by our simulators where a simulator exists. Not yet tested on a phone: 9-pin no-tap, the replay-rate fix and the invisible-oil fix (1.7.2), and the library row layout (1.7.1).

## 7. Keys

Do not share BowlingPlus's Android signing key or its password. The team should sign its own builds.

## Appendix A: how BowlingPlus does it at runtime (reference only)

You don't need this to build the features. It's here so you can check our claims.

- **Frame rate:** BowlingPlus writes to the static fields of `Client.Core.Constants` and calls `Application.targetFrameRate`. The iOS plist key is set in the IPA. See `features/120-fps.md`.
- **DNS:** BowlingPlus rewrites the game's `getaddrinfo` import slot in memory (a "fishhook" rebind on iOS, a GOT rewrite on Android) so game hosts resolve to IPv4. See `features/ipv6-loading-fix.md`.
- **Pins and physics:** BowlingPlus sets the material friction on the lane's pin colliders at runtime, and changes the engine's fixed step in memory. The runtime step change is the riskiest part, and the one we'd least recommend copying.
- **Gating:** `gBFStatus.offline` is a flag set from the game's mode, and each feature checks it.
- **Offsets and names:** the IL2CPP addresses in the feature documents are for the shipped binary. They don't apply to your source.
