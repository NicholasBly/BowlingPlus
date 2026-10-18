# BowlingPlus test plan

Two parts: checks that run on a computer (on every push in GitHub Actions, or locally in a couple of minutes), and
a list to go through on a phone before a release. The first part runs the live code: the checks cut the real
functions out of `android/native/Game.cpp`, `src/MenuButton.mm` and `ButtonSpot.java` and run them against fake
games and screens, so they test what ships, not a copy. They can't test what the game itself does (that's what
the second part is for), and every fake says which game behaviour it assumes.

## Part 1: on a computer

`sh tools/tests/run_all.sh` (needs python3 and g++; javac/java for the Android half of the menu button check).
GitHub Actions runs the same script on every push and pull request (`.github/workflows/checks.yml`); the iOS and
Android build workflows also compile both platforms for real (Theos with the iOS SDK, Gradle-free Android build
against the SDK's own android.jar).

| Check | What it catches |
|---|---|
| `tools/tests/static_checks.py` | A shared block (the code both platforms run, byte for byte) that drifted on one side; a setting saved but never read back, read but never saved, set from the Android menu but unknown to the native side, or present on one platform only; the settings struct out of step; the version not the same in `control`, both `BFShared.h`, `BF.java`, the changelog and the README; a workflow that doesn't parse or runs a file that isn't there; a Python tool that doesn't compile. |
| `tools/dev/host_check.sh` | Android native code that doesn't compile or link (stub JNI/Android headers, `-Wall -Wextra`, format strings checked), or exports more than `JNI_OnLoad`. |
| `tools/dev/ios_order_scan.py` | An iOS static function used before it's declared (broke the 1.6.1 iOS build). |
| `tools/dev/pinsim/run.py` | Pin turns: 41 checks against a lane that racks like the game's decoded `UpdatePinPositions`. |
| `tools/tests/rack/run.py` | Obstacles: a new layout only for a new rack. Whole games through a fake game whose `_kegsUp` behaves like the real one (written only when it racks; after a strike all ten up directly): strikes, open frames, redos (a ball off the lane), gutters, the 10th frame (strike, spare, gutter, open), a pinsetter that moves the pins late, scene changes, a kind switched off, restarts, the game's frame number moving late, and the frame fields not found. Mutation-checked: the rule up to 1.7.6's second build fails 6 of its checks. |
| `tools/tests/stools/run.py` | Barstools: 187,488 throws over every stool layout (30-144 Hz, 4-10.5 m/s, straight and hooking both ways, 31 aims across each stool) with a fake ball the engine moves by its velocity: every ball that comes within reach is bounced, never goes on into the stool, never heads back up the lane more steeply than 1 in 2 or more than 1 m, never speeds up, and ends in the gutter before the pins; a ball that stays clear is never bounced. Against the second 1.7.6 test build's code it finds 1,251 balls rolling through a stool and 132 crossing one into the gutter. |
| `tools/tests/ball/run.py` | Loft: 9,300 releases (1-16 ft, 4-11 m/s, let go 0-40 cm up) land within 1 cm of the setting or are held by the 1 m cap, never aimed down faster than 1 m/s. Landing: 1,200 landings at 30-144 Hz at any phase, every hop of 2 cm or more caught before its top; a rolling ball never touched. |
| `tools/tests/button/run.py` | The menu button's spot on six iPhones (Dynamic Island, notch, neither) and six Android screens (punch hole, notch, corner hole, status bar, content below the cutout): always on screen, never over the island or cutout, a saved spot stays put, up to the top away from the cutout, held below the top over no more than the cutout's width. |

When the game updates, also rerun the tools that read its binaries (`tools/dev/README.md`): `pinhook/check.py`,
`physrate/check.py`, `il2cpp_meta.py`, and `ramps/make_ramps.py --check-only` (needs pinlab built).

A new shared block goes in the `SHARED` list in `static_checks.py`; a new setting needs its key in both
`Engine.mm` (load and save) and `Engine.cpp` (`BFConfigJson` and `BFConfigSet`). Logic that can be tested on a
computer is easier to keep testable as a small function with plain arguments (`RoRackDecide`, `LoftVz`,
`DampBounced`, `BFButtonSpot`, `ButtonSpot.clamp`).

## Part 2: on a phone

Go through this on one iPhone and one Android phone with the build that's about to ship. For each problem: what
you did, what you saw (a screen recording helps most, slowed down for anything that lasts a frame), and **Copy
debug info** right after it.

### Ramps & obstacles (Practice, game mode Ramps & obstacles)
1. **No re-rolls.** Throw a ball off the far edge onto the next lane (the game gives the shot back): the layout
   stays. Do it three times. Throw a first ball into the gutter: the second ball gets the same layout. Debug info:
   `same rack` goes up each time, `builds` doesn't.
2. **A new layout for each new rack:** after every strike and every finished frame. In the 10th frame: a new one
   after a strike or a spare, the same one after a gutter or a redo. Debug info: `last-frame pins knocked` goes up
   after a 10th-frame strike or spare.
3. **Leave and come back mid-frame** (open the pattern screen, or go to the menu and back): the same layout comes
   back for the second ball.
4. **Barstools:** roll at both edges of each stool and straight at the middle, slow and fast, and between the two
   stools of a pair side by side (Squeeze, The gate, Back gate): it bounces off into the gutter every time (head-on:
   back a little and to the side), never through and never stuck to a stool's front (`stool passes` stays 0). The sweeper's blue banner is never drawn over a
   stool, its pin or a gate (with obstacles on the lane the banner may show the bar's frame over its ends again, as
   before 1.6.9).
5. **Gates and ramps:** a ball through the opening scores; a ball off a ramp's edge or into a gate scores 0.
6. **Replays** of each kind show the layout the shot was bowled on, and the ball never shows up away from where it
   was (watch the first frames: no ball floating behind the foul line).

### Throwing, loft and landing (Practice)
1. With loft 0 and with loft 4, 8 and 16 ft, with Ramps & obstacles on and off: the ball leaves the hand at the hand
   (record at the highest frame rate and step through the release). Debug info `damped` only goes up after a loft
   landing, not on ordinary throws.
2. A lofted ball lands about where the setting says (count boards/arrows) and doesn't hop after landing. A slow
   ball with a long loft comes down short (`capped` goes up).
3. Backup ball: the hook goes the other way, speed and RPM unchanged.

### Menu button (Display & startup: on-screen menu button)
1. Drag it slowly along the very top edge from one side to the other: it goes up to the top on both sides of the
   Dynamic Island / notch / camera hole and slides round it under the middle, without sticking at a line lower down.
2. Leave it at the top left, close and reopen the game: it's in the same place. Same for the top right and a spot
   under the island.
3. Android: with the status bar showing and hidden (swipe down from the top, let it hide).

### High frame rate (Display & startup)
1. The FPS counter shows the screen's top rate (the line under the switch says what the phone reports). iOS needs
   the patched IPA (`plist120=1`).
2. Android: open the menu and scroll: smooth at the top rate, with High frame rate on and off.

### Smoke test of everything else
Spare mode, pin physics and double rate (first-ball log lines), custom oil and the oil library, oil breakdown and
thickness, invisible oil and 9-pin no-tap modes, your own pin and background pictures, arsenal sort, back up my
data, fix connection, safe mode (force-close twice during startup), privacy page, online play untouched (BowlingPlus
stays out of online games).
