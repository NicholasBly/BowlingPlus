# BowlingPlus 🎳

A tweak for **Bowling by Jason Belmonte** (made for v1.907) that fixes two long-running bugs and adds a fun menu for offline Practice. Works on a normal, non-jailbroken iPhone.

Nothing shows up on screen until you **shake your phone**. Shake again (or tap outside the menu) to close it.

## What's in it

| Feature | What it does | Where it works |
|---|---|---|
| Match Up skin fix | Match Up Pearl and Match Up BP get their real skins on the rack, in the Arsenal, previews, your hand and replays | Everywhere (looks only) |
| Pin physics fix | Fast or spinning pins can't pass through other pins, and pins clipped low at the base can tip over | Practice only |
| Ball speed | Slider from 1x to 5x | Practice only |
| Spare shooting mode | Pick which pins are standing at the start of every frame, or auto-rack the same pins every frame | Practice only |
| Arsenal search | Filter your Arsenal by ball name | Everywhere |
| Skip tutorial | A button during the first-launch tutorial that runs the game's own skip, so you can log in right away | Tutorial only |
| 120 FPS mode | Menus and gameplay at 120 FPS on 120 Hz screens (the game normally uses 30/60). Off by default | Everywhere |
| Auto-accept privacy popup | Presses "Sign up" on the My.Games privacy page that shows every launch. Never presses updated-terms pages. Off by default | Startup |
| Don't get stuck connecting | Shows the game's gray offline button after 30 s stuck connecting, and hides a loading circle that's been up 30 s. On by default | Everywhere |
| Game server over IPv4 | The game always picks its servers' IPv6 addresses, which don't answer (hangs with NextDNS). Uses IPv4 instead. On by default | Everywhere |
| Fix oil display side | The game draws oil mirrored, so breakdown/carrydown showed on the wrong side. On by default | Everywhere (looks only) |
| Show oil breakdown | Redraws the lane oil after every shot | Practice |
| Invisible oil | Hidden oil, random unlocked game pattern each game | Practice |
| Custom oil patterns | Real Kegel-style forward/reverse steps, built by the game's own engine; share via QR | Practice |
| Copy debug info / Copy log | Copies what the tweak sees, plus a log of loading, the connection and network checks, for bug reports | Menu |

The Practice-only stuff turns itself off in online matches, tournaments, and the tutorial, so it never messes with other players.

## Install (no jailbreak)

**Option A: Sideloadly**
1. Download the latest .ipa (prepackaged iOS app with the tweak applied) from Releases.
2. Open Sideloadly and pick the app.
3. Connect your iDevice to your PC/Laptop/Mac via USB data cable.
4. Under the "iDevice" dropdown, your device should show.
5. In settings, select local/remote and Apple ID Sideload.
6. Start. It should proceed to install the app onto your device.

**Option B: Sideloadly (inject it yourself)**
1. Open Sideloadly and pick your game IPA.
2. Click **Advanced options**.
3. Find **Inject dylibs/frameworks** and add `BowlingPlus.dylib`.
4. Hit **Start** like normal.

**Option C: Signulous (or any app that just signs IPAs)**
Use an IPA that already has the tweak inside (see "Make a patched IPA" below), then install it like any other app.

## How to use it

- **Shake** to open the menu.
- **Arsenal search:** type part of a name (like `match up`) and tap **Search**. Open the Arsenal and only matching balls show up. **Clear** brings everything back.
- **Ball speed:** drag the slider. The boost kicks in right after you let go of the ball. **Reset to 1x** turns it off.
- **Spare shooting mode:** turn it on and start a Practice game. At the start of each frame a pin picker pops up. Tap pins on/off (or use a preset like 7-10), then hit **Rack 'em**. **Full rack** skips it for that frame.
- **Auto-rack:** in the pin picker, tap **Auto (every frame)** instead of Rack 'em. Those pins get set up every frame without asking. Shake for the menu and turn off **Auto-rack** to get the picker back.
- **Skip tutorial:** on a fresh install, a **Skip tutorial** button shows at the top right during the tutorial. It uses the game's own skip, then you can log in.
- **120 FPS mode:** shake for the menu, turn on **120 FPS mode** under Display & startup. The line under the switch tells you if it's working. It needs the patched IPA from this repo (it allows 120 Hz in Info.plist); a dylib-only install stays at 60 Hz.
  - Heads up: pins you removed count as already knocked down, so the scoreboard will look weird (clearing your pins can show up as a strike). Scores in this mode are just for fun.
- Your settings are saved.

## How the fixes work (short version)

- **Pins:** every pin uses Unity's cheapest collision check ("Discrete"), and the game's collision padding is 20x thinner than Unity's default. A fast pin can move several centimeters between physics checks and skip right through another pin's skinny neck. The fix switches the pins (and the ball) to "Continuous Dynamic", which checks the whole path between steps.
- **Match Up Pearl:** each ball's store entry links to a skin file in the game's own asset bundles, and a flag called `texc` picks which half of that file to show (most files hold two balls). The Pearl's entry points at the wrong file, the same black look as the BP. Its real art is the left half of `Text_MatchupPearl_MatchupHybrid`, the file the Match Up Hybrid uses. The tweak checks that file's name in the game's skin catalog, points the Pearl at it (in memory only), and picks the half the Hybrid doesn't use. After that, the game's own code draws it right everywhere. Turning the switch off puts the game's original data back.
- **Pins hit low:** pins ran on Unity's old default spin limit (7 rad/s), so 1.1.0 raised it to 50. 1.2.x adds an experimental pins-only option for speculative collision checks (never the ball: on the ball it caused jumps), because the v1.1 sweep checks only follow straight-line motion: a tumbling pin's top could end up inside another pin before the hit was noticed (the game's contact margin is only 0.5 mm, and Unity removed its setter).
- **Arsenal search:** the game rebuilds the list with `ResetScroll()`, which re-counts the balls. v1.0 used `ReloadData()`, which only redraws what's on screen, so the scroll area kept its old size.
- **Match Up BP:** its store entry points at `black_pearl2_dif_x512.jpg`, a file that isn't in any of the game's asset bundles, so it never loads (white on the rack, or the last ball's skin in your hand). The real Black Pearl art does ship, as the left half of `tex_blackpearl_flameturquoise.png` (the file the game had put on the Pearl), so the tweak points the BP there. A drawn look-alike is only a last resort if no working skin can be found.
- **Speed:** the game launches the ball with one push, and the hook comes from friction with the oil. The tweak waits for the launch and multiplies the speed once.
- **Spare mode:** the game already re-racks from a 10-pin "standing" list for your second ball. The tweak edits that list at the start of a frame and re-racks.

Everything is found by name while the game runs (no hard-coded memory addresses), so small game updates usually keep working.

## Build it yourself on GitHub

1. Make a new GitHub repo and upload everything in this folder. **Don't upload the game IPA** (it's the developer's app).
2. Go to the **Actions** tab, pick **Build BowlingPlus**, and hit **Run workflow** (it also runs on every push).
3. When it's done, open the run and download the **BowlingPlus** artifact. It has `BowlingPlus.dylib` (plus a `.deb`).

## Make a patched IPA (for Signulous)

You just need Python 3:

```
python3 tools/inject_ipa.py YourGame.ipa BowlingPlus.dylib BowlingPlus.ipa
```

Then sign/install `BowlingPlus.ipa`.

## If something's off

- **Menu won't open:** give the phone 2-3 quick, firm shakes, a few seconds after the game finishes loading.
- **See what the tweak is doing:** on a Mac, open the Console app, pick your iPhone, and search for `BowlingPlus`.
- **Reporting a problem:** tap **Copy debug info** in the menu and paste it into your message.
- **After a game update:** the log may say "class not found" or "field not found". That means the game renamed something.
- **Safe mode:** if the game fails to finish starting twice in a row, BowlingPlus pauses itself so the game still works. Shake to open the menu and tap **Turn BowlingPlus back on**.
- **A Korean "약관 및 개인정보 취급방침" screen on first launch:** that's the game's own privacy consent screen (from its publisher's SDK), not BowlingPlus. It shows on any brand-new install, including Signulous "duplicate app" installs.
- **"Duplicate app" installs** start as a fresh player with their own separate save data. Your normal copy of the game isn't touched.

## Changes

Short version below. Full list in [CHANGELOG.md](CHANGELOG.md).

- **1.4.2:** custom patterns drawn correctly (BowlingPlus copy of the game's Kegel engine, using the sheet's distances), the original oil look by default, and an oil color picker.
- **1.4.1:** replays keep your custom oil, patterns carry their reverse brush drop, Show oil thickness, and an oil report in debug info.
- **1.4.0:** renamed to BowlingPlus, the BowlingPlus collection (Baton Rouge), thickness-colored previews, working ⋯ and Start from panels, and custom-oil overlay and replay fixes.
- **1.3.1:** no more blank flash when swiping patterns, the custom pattern ⋯ menu works, and a Baton Rouge (2012 USBC Open) pattern QR.
- **1.3.0:** oil: fixed the mirrored oil display, show breakdown, invisible oil, and custom Kegel patterns with QR sharing.
- **1.2.3:** fixed NextDNS hangs (game servers don't answer on IPv6, and the game always picked IPv6), plus a time limit for the loading circle.
- **1.2.2:** diagnostics log with **Copy log** and **Run connection test**, to track down the NextDNS loading hang.
- **1.2.1:** fixed the ball jumping over the pins (1.2.0), added a fix for endless loading with NextDNS, and made spinning-pin hits a pins-only experiment (off by default).
- **1.2.0:** 120 FPS mode, privacy-popup auto-accept, better spinning-pin hits, and no more kick back to the menu after skipping the tutorial.
- **1.1.2:** Match Up BP skin fixed (it pointed at a file that isn't in the app). Skin lookups use the game's real catalog. Added `VERIFIED_NOTES.md`.
- **1.1.1:** Match Up Pearl skin actually fixed (its store entry pointed at the BP's black skin; v1.1.0 mistook that for "already fixed"). One Match Up skins switch instead of two. Skip tutorial button no longer sticks around after skipping. Debug info lists the skin files.
- **1.1.0:** speed max 5x, spare mode auto-rack, skip tutorial button, Arsenal search scrolling fixed, pins can tip when hit low (spin limit 7 -> 50), copy debug info, BP look-alike checked every frame.
- **1.0.1:** fixed the freeze on the loading screen (game handles were stored in 32 bits instead of 64). BowlingPlus now waits until the game has fully loaded before doing anything, never calls game code that creates things, and has a crash guard (safe mode).
- **1.0.0:** first version.

## Files

| File | What it is |
|---|---|
| `Tweak.xm` | Starts the tweak + catches the shake |
| `src/Il2Cpp.*` | Talks to the game's code by name |
| `src/Game.mm` | All the fixes and fun features |
| `src/Menu.mm` | The shake menu and the pin picker |
| `src/Shake.mm` | Backup shake detector (accelerometer) |
| `src/BPTexture.mm` | Draws the Match Up BP look-alike skin |
| `src/Engine.mm` | Runs things every frame + saves settings |
| `tools/inject_ipa.py` | Puts the dylib inside an IPA |
| `CHANGELOG.md` | Everything that changed, version by version |
| `VERIFIED_NOTES.md` | Verified facts about the game (names, offsets, how skins load) for future work |

For personal use. Not affiliated with the game's developers or Jason Belmonte.


## Links

- Project: https://github.com/NicholasBly/BowlingPlus
- Support development: https://github.com/sponsors/NicholasBly
