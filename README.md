# BowlingPlus 🎳

A tweak for **Bowling by Jason Belmonte** (made for v1.907) that fixes two long-running bugs and adds a fun menu for offline Practice. Works on a normal, non-jailbroken iPhone.

Nothing shows up on screen until you **shake your phone**. Shake again (or tap outside the menu) to close it.

## What's in it

| Feature | What it does | Where it works |
|---|---|---|
| Match Up skin fix | Match Up Pearl and Match Up BP get their real skins on the rack, in the Arsenal, previews, your hand and replays | Everywhere (looks only) |
| Pin physics fix | Fast or spinning pins can't pass through other pins, and pins clipped low at the base can tip over | Practice only |
| Ball speed | Slider from 1x to 5x | Practice only |
| Ball spin (RPM) | Slider from 1x to 17x: past the game's ~600 rpm cap, up to about 10,000 rpm, with the extra grip (hook) that much spin gives | Practice only |
| Spare shooting mode | Pick which pins are standing at the start of every frame, or auto-rack the same pins every frame | Practice only |
| Tap the pin layout | Tap the top-right pin layout (holding the ball) or the little screen under the ball return (overhead view) to pick pins for just that one shot, without turning on Spare mode. The picker has an X to leave without changes | Practice only |
| Arsenal search | Filter your Arsenal by ball name | Everywhere |
| Skip tutorial | A button during the first-launch tutorial that runs the game's own skip, so you can log in right away | Tutorial only |
| 120 FPS mode | Menus and gameplay at 120 FPS on 120 Hz screens (the game normally uses 30/60). Off by default | Everywhere |
| Privacy popup, once | Accept the My.Games privacy page yourself one time. From then on it is hidden the moment it appears and its Sign up button is pressed for you, so you never see it again. Updated-terms pages are never pressed and are left for you to read | Startup |
| Don't get stuck connecting | Shows the game's gray offline button after 30 s stuck connecting, and hides a loading circle that's been up 30 s. On by default | Everywhere |
| Game server over IPv4 | The game always picks its servers' IPv6 addresses, which don't answer (hangs with NextDNS). Uses IPv4 instead. On by default | Everywhere |
| Fix oil display side | The game draws oil mirrored, so breakdown/carrydown showed on the wrong side. On by default | Everywhere (looks only) |
| Show oil breakdown | Redraws the lane oil after every shot | Practice |
| Invisible oil | Hidden oil, random unlocked game pattern each game | Practice |
| Custom oil patterns | Real Kegel-style forward/reverse steps, drawn by BowlingPlus's copy of the game's oil engine; share via QR | Practice |
| Real-life oil (always on) | In Practice the game's own 48 patterns and yours are drawn from their real Kegel data, not the game's simplified engine: exact distances on the lane's quarter-foot rows, microliters per step, reverse oil adding on top, the brush carrying oil back to the foul line, and Kegel's brushed film over the whole lane (details below). The game's own oil is back whenever you're not in Practice | Practice |
| (how it works) | Custom patterns lay oil like the sheet: exact distances on the lane's quarter-foot rows, microliters per step, reverse oil adding on top, the brush carrying oil back to the foul line, and Kegel's brushed film over the whole lane. Checked against four Kegel charts (99.9% of cells). On by default | Practice |
| Kegel pattern import | Pick a pattern downloaded from the Kegel Pattern Library app or website (the .pdf data sheet, or a .zip / .Pattern / .txt) and it's added: steps, distances and drop brush | Practice |
| BowlingPlus collection | Real Kegel patterns ready to play: Baton Rouge (2012 USBC Open), PBA Regional 37 (2026) | Practice |
| Oil color | Pick the color the lane shows oil in, or keep the game's | Everywhere (looks only) |
| Your own pin image | Put your own picture on the pins (all lanes). Draw on the 2:1 wrap template: one sheet that wraps around the pin like paper, so no seams | Everywhere (looks only) |
| Copy debug info / Copy log | Copies what the tweak sees, plus a log of loading, the connection and network checks, for bug reports | Menu |

The Practice-only stuff turns itself off in online matches, tournaments, and the tutorial, so it never messes with other players.

## Install (no jailbreak)

Prerequisites:
Enable Developer Mode on your iDevice (Settings -> Privacy and Security, scroll to the bottom to find Developer Mode, turn it on)
After sideloading the app, you will need to trust the developer (me). Settings -> General -> VPN and Device Management, find developer and click trust.

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

- **Shake** to open the menu. It's grouped into cards (tap a card's title to fold it). Descriptions are hidden to keep it short: tap the **ⓘ** at the top to show them all, or tap a row's title to show just that one.
- **Arsenal search:** type part of a name (like `match up`) and tap **Search**. Open the Arsenal and only matching balls show up. **Clear** brings everything back.
- **Ball speed:** drag the slider. The boost kicks in right after you let go of the ball. **Reset to 1x** turns it off.
- **Ball spin (RPM):** same idea for spin. 17x takes a 600 rpm throw to about 10,000 rpm, and the extra revs add grip, so expect big hook.
- **Preview a pin picture on your PC:** open `pins/BowlingPlus-pin-preview.html` in a browser and drop the picture on it.
- **Your own pin image:** in the menu, tap **Get the wrap template + guide**, draw your design on the 2048 x 1024 template (left to right = once around the pin; the left and right edges meet), then tap **From Photos** or **From Files** and pick it.
- **Spare shooting mode:** turn it on and start a Practice game. At the start of each frame a pin picker pops up. Tap pins on/off (or use a preset like 7-10), then hit **Rack 'em**. **Full rack** skips it for that frame.
- **Pick pins for one shot:** in Practice, tap the pin layout in the top right while you hold the ball, or the little screen under the ball return in the overhead view. The pin picker opens, starting from the pins that are standing now. Tap pins, hit **Rack 'em**, and they're set up right now. It only lasts until you throw: the next frame is a normal full rack unless Spare shooting mode is on. The **X** closes the picker without changing anything (this works in Spare mode too).
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

- **Updates:** tap **Check for updates** at the bottom of the menu. It asks GitHub for the release marked **Latest**.
- **Menu won't open:** give the phone 2-3 quick, firm shakes, a few seconds after the game finishes loading.
- **See what the tweak is doing:** on a Mac, open the Console app, pick your iPhone, and search for `BowlingPlus`.
- **Reporting a problem:** tap **Copy debug info** in the menu and paste it into your message.
- **After a game update:** the log may say "class not found" or "field not found". That means the game renamed something.
- **Safe mode:** if the game fails to finish starting twice in a row, BowlingPlus pauses itself so the game still works. Shake to open the menu and tap **Turn BowlingPlus back on**.
- **A Korean "약관 및 개인정보 취급방침" screen on first launch:** that's the game's own privacy consent screen (from its publisher's SDK), not BowlingPlus. It shows on any brand-new install, including Signulous "duplicate app" installs.
- **"Duplicate app" installs** start as a fresh player with their own separate save data. Your normal copy of the game isn't touched.

## Changes

Short version below. Full list in [CHANGELOG.md](CHANGELOG.md).

- **1.6.1:** Android fixes: menu no longer scrolls at a crawl (the game drops to 30 FPS while a panel is open), the oil library and pin picker are centered, the footer is centered, the on-screen button stays out from under panels and now remembers its setting, plus fewer freezes and a fixed build error. iOS is unchanged.
- **1.6.0:** redesigned shake menu (cards, descriptions on demand), privacy popup remembered after one accept, Kegel-accurate oil everywhere in Practice (also the game's own 48 patterns), tap the pin layouts to pick pins for one shot, an X on the pin picker, and the footer no longer cuts off "Check for updates".
- **1.5.3:** custom oil now matches Kegel's charts: exact distances (no more overlapping rectangles), film on every board (no more bare strips), and no oil inflation.
- **1.5.2:** Kegel-accurate oil carries reverse oil back to the foul line like a real machine (the front of the lane was too thin).
- **1.5.1:** Kegel-accurate oil for custom patterns (microliters per step), a PC pin picture preview page, and a guide to the pin picture system.
- **1.5.0:** the RPM boost now adds hook (extra grip), and pin pictures use the one-piece wrap sheet.
- **1.4.9:** ball spin (RPM) boost up to about 10,000 rpm.
- **1.4.8:** wrap layout for pin pictures (seams always match), no crack near the base, the IPv4 fix actually attaches now, safe mode only counts real crashes, and "Pins face random ways" removed (the game already does it).
- **1.4.7:** your pin image no longer pops in at each new rack, and no crack down the side of the pins.
- **1.4.6:** use your own pin image, with a layout guide and a template.
- **1.4.5:** pins face random ways when set (like real life) and spin around their own axis in flight.
- **1.4.4:** Kegel PDF import (what the Kegel app and website download), and skipped parts of a pattern get a thin film instead of full oil.
- **1.4.3:** Kegel pattern file import (.zip/.Pattern/.txt), PBA Regional 37 in the collection, custom patterns on the correct side of the lane with no bare gaps, Check for updates, and menu layout fixes.
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
| `src/Menu.mm` | The shake menu, the pin picker, and the tap listener for the pin layouts |
| `src/Privacy.mm` | Remembers the privacy page once, then hides it and presses it for you |
| `src/KegelParse.h` | Reads Kegel's text pattern files (the game's 48 patterns and `.txt` imports) |
| `src/Shake.mm` | Backup shake detector (accelerometer) |
| `src/BPTexture.mm` | Draws the Match Up BP look-alike skin |
| `src/OilUI.mm` | Custom oil: pattern library, editor, QR share, Kegel file import, oil color picker |
| `src/OilCollection.mm` | The BowlingPlus collection of real Kegel patterns |
| `patterns/README.md` | How to add a pattern from a Kegel data sheet |
| `pins/` | Pin picture guides and templates (wrap and game layout), 3D checks, and `BowlingPlus-pin-preview.html` (preview your pin picture on a 3D pin on your PC) |
| `tools/oil_verify/` | Checks the oil model against Kegel's PDF charts, cell by cell |
| `PIN_TEXTURE_SYSTEM.md` | How the pin picture system works, in detail (for developers and AI agents) |
| `src/Engine.mm` | Runs things every frame + saves settings |
| `tools/inject_ipa.py` | Puts the dylib inside an IPA |
| `CHANGELOG.md` | Everything that changed, version by version |
| `VERIFIED_NOTES.md` | Verified facts about the game (names, offsets, how skins load) for future work |

For personal use. Not affiliated with the game's developers or Jason Belmonte.


## Links

- Project: https://github.com/NicholasBly/BowlingPlus
- Support development: https://github.com/sponsors/NicholasBly
