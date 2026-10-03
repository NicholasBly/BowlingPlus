# Changelog

Every change to BowlingFix, newest first.

## [1.4.4] - 2026-10-02

### Added
- **Import Kegel PDFs.** The Kegel Pattern Library app and website download a pattern as its PDF data sheet. **Import a Kegel pattern file** now reads it: steps, distances, drop brush and name. Zips that only hold a PDF work too.

### Fixed
- **Skipped stretches of a board got too much oil.** 1.4.3 blended the oil across them, which merged patterns like Kegel's "Start 5 and Stop 15" calibration pattern (alternating 2 ft bars) into solid blocks. They now get a thin buffed film, the way Kegel draws them, so the bars stay bars.

## [1.4.3] - 2026-10-02

### Added
- **Import Kegel pattern files:**
  - **What to pick:** in the pattern library, tap **Import a Kegel pattern file** and pick a pattern downloaded from the Kegel Pattern Library: the `.zip` itself, or the `.Pattern` / `.txt` inside.
  - **What you get:** the exact steps, distances and drop brush. No typing by hand.
- **2026 PBA Regional 37** in the BowlingPlus collection, from the official Kegel file.
- **Check for updates** at the bottom of the menu, next to Donate. It asks GitHub for the release marked Latest and tells you if there's a newer one (tap to open it).

### Fixed
- **Custom patterns had bare spots.** When a pattern narrows and then widens again (Regional 37 does), the game's oil engine leaves the boards in between completely dry. BowlingPlus now fills them with carried-forward oil, like a real lane and Kegel's chart.
- **Lopsided custom patterns were on the wrong side.** The game draws Kegel's left boards on the bowler's right. Custom patterns now match the sheet: left is left. Previews are drawn the same way (the top edge is the bowler's left).
- **Typing in Arsenal search squeezed the Clear button away.**
- **The menu's ✕ wasn't at the right edge.**
- **Pattern names in "Start from"** showed the game's formatting codes (`<size=50>…</size>`).
- **Step distances are kept to two decimals** (were rounded to 0.1 ft), so imported patterns are exact.

## [1.4.2] - 2026-10-02

### Fixed
- **Custom patterns were drawn in the wrong place** (your Baton Rouge staircase sat at the far end of the lane). The game's own step-adding code (from a pattern editor hidden in the game):
  - silently dropped forward travel steps, so the pattern ended at 28 ft instead of 39
  - put the first reverse step at a setting borrowed from another pattern, which pushed all the reverse oil 16 ft down the lane

  BowlingPlus now draws custom oil with its own copy of the game's Kegel engine, checked against the oil report from your phone (every value matched), using each step's distance straight from the sheet.
- **The oil looks like the game's again.** "Show oil thickness" is now off by default (your 1.4.1 setting is reset).

### Added
- **Oil color** (Oil section of the shake menu): a color slider, presets, and "Use the game's color" to go back.
- The oil report in "Copy debug info" now includes a self-check: BowlingPlus draws the current game pattern with its own engine and reports any difference from the game's drawing.
- Patterns copied from the game or the collection keep their exact step distances until you change a step's loads or speed. Share codes carry this too.

## [1.4.1] - 2026-10-02

### Fixed
- **Replays showed the original pattern instead of your custom oil** (and invisible oil showed the real pattern). While a replay plays, BowlingPlus thought you'd left practice and put the original oil back. A replay now counts as part of your practice game.
- **Custom patterns could lose their reverse oil.** The game's engine only lays a reverse step's oil if it ends short of the pattern's **reverse brush drop**, and custom patterns were inheriting that number from whatever pattern they were built from. Patterns now carry their own brush drop:
  - Baton Rouge uses 34 ft, from its sheet.
  - The editor has a stepper for it.
  - Share codes include it.

### Added
- **Show oil thickness** (on by default; Oil section of the shake menu). The game's lane colors look the same for any oil thicker than a thin film. This shades oil by thickness in your oil color: darker = more oil. Looks only; the ball feels exactly the same oil.
- **Oil report in "Copy debug info":**
  - The brush drop used and the engine's end distance for every step.
  - Oil every 5 ft on an outside, mid and center board.
  - What the engine computed next to what's on the lane right now.

## [1.4.0] - 2026-10-02

### Changed
- **BowlingFix is now BowlingPlus.** Your settings and saved oil patterns carry over automatically.
- The shake menu ends with the BowlingPlus logo, a link to the GitHub project, and a donation link.

### Added
- **BowlingPlus collection:** real Kegel patterns from their data sheets, playable right away by everyone. The first is **Baton Rouge (2012 USBC Open Championships)**. Collection patterns can be shared, or copied to make your own edits.
- **Oil thickness you can see:**
  - Pattern previews now color oil by thickness, light cyan = thin to navy = thick, like a Kegel sheet, with a legend.
  - The game's own lane colors barely change above a few units of oil, which is why every pattern looked one color. The ball still feels the real thickness.

### Fixed
- **The ⋯ button on a pattern did nothing**, and neither did **"Start from"** in the editor. iOS pop-up menus don't open inside the game. Both now use BowlingPlus panels:
  - **⋯:** Edit, Share / export QR code, Copy code, Duplicate, Delete (tap twice to confirm).
  - **Start from:** pick any game pattern or collection pattern.
- **Custom oil looked different depending on which pattern it replaced.** It was built on top of that pattern's machine settings. It's now always built the same way, so it looks and plays the same over any pattern.
- **Custom oil stayed on a pattern's preview after you turned it off or swiped away.** The game keeps a cached picture of every pattern's oil, and only the current pattern's picture was being redrawn. Now the exact pattern that changed gets redrawn, both when custom oil goes on and when it comes off.
- **Shot replays didn't show the oil.** The replay draws the recorded oil into its own picture, which the oil-side fix wasn't preparing. It now is, so replays show the oil your ball actually rolled through, custom or not.
- The 1.3.1 "prepare every oil picture" step looked pictures up by the wrong number. It now uses the list position, like the game.
- The share sheet and photo picker now open from a small window of BowlingPlus's own, so they don't depend on the game's screens.

## [1.3.1] - 2026-10-02

### Fixed
- **Blank oil flash when swiping the pattern carousel.** The oil-side fix needs each pattern's oil texture set to Repeat, but the game creates a texture (on Clamp) the first time it shows a pattern, and it stayed blank until BowlingFix's next check. Now every pattern's texture is prepared once in the background, and the lane's texture is checked every frame.
- **The ⋯ button on a custom pattern did nothing.** The row's "tap to select" handler swallowed the tap. The button now opens Edit / Share QR code / Duplicate / Delete, uses the standard ⋯ icon, and the library says what it's for.
- Editor: a travel-only step can now travel back to 0 ft (needed for reverse passes), and only forward steps get a 40 ft starting value.

### Added
- **Baton Rouge (2012 USBC Open Championships)** as a ready-made custom pattern (QR image + text code), read from the Kegel data sheet. Its 10 forward and 7 reverse steps match the sheet's 385/119 boards crossed and 25.2 mL.

## [1.3.0] - 2026-10-02

### Fixed
- **Carrydown and breakdown shown on the wrong side of the lane.** The lane's oil shader draws the oil grid mirrored left/right compared to the physics. The physics itself was always right: the ball reads and moves oil where it really rolls. **Fix oil display side** (on by default) flips the drawing back, so the ball track, breakdown and carrydown show up where your ball went.

### Added (practice)
- **Show oil breakdown:** the game really does break down oil every shot (the ball drags 20% of the oil along its path) and only resets it each new game. This redraws the lane after every shot so you can watch it happen.
- **Invisible oil:** hides the oil and plays a random **unlocked** built-in pattern each game (never custom ones). Turning it off puts your pattern back.
- **Custom oil patterns:**
  - **Opening:** a "Custom oil ▾" tab at the top of the practice pattern screen (tap or swipe down), or from the shake menu.
  - **Real Kegel steps:** patterns use the same forward/reverse load steps as a real Kegel lane machine (start/stop board in L/R notation, loads, speed, and travel-only steps). They're built by the game's own Kegel engine, so they behave exactly like the built-in patterns.
  - **Editor:** start from any of the game's 48 patterns, with a live preview in the game's oil colors and the distance each step reaches.
  - **Sharing:** export as a QR code or text code; import with the camera, a screenshot, or paste.
  - **Practice only:** your pattern replaces the one you start, and it's undone the moment you leave practice.
- The patched IPA now has a correct camera permission message (the game's said "photo library"), used for scanning pattern QR codes.

### Changed
- Adopted your reworded menu (concise descriptions) as the new baseline.

## [1.2.3] - 2026-10-01

### Fixed
- **Hangs with NextDNS (and any DNS that returns IPv6 addresses).** The 1.2.2 log found the cause:
  - The game's network library (Photon) always connects to the **first IPv6 address** it gets for a server, and only uses IPv4 if there is no IPv6 at all.
  - The game's servers **don't answer on IPv6** (tested: no answer over IPv6, instant answer over IPv4).
  - NextDNS returns IPv6 addresses, so connections hung, timed out and retried forever.
  - New **Game server over IPv4** switch (on by default): the game's lookups for its own servers return IPv4 only. It works by filtering the game's DNS calls in memory, the "fishhook" technique, which only changes data, never code. If a server has no IPv4 address, the normal lookup is used.
- **Loading circle that never goes away** (seen after tapping play in the practice lobby). The game waits for a server answer with no time limit. **Don't get stuck connecting** now also hides a circle that's been up for 30 s, so you can try again.

### Added
- Diagnostics: the log now shows whether the game connection uses IPv6, how many loading circles are up, whether the DNS filter got installed, and connection tests for the actual game servers (`w1`/`p1.wannaplay.studio:4056` and the raw IPv4 relay).

## [1.2.2] - 2026-10-01

### Added
- **Copy log** (shake menu, Help). It copies a diagnostics log recorded from the moment the game starts:
  - **Game state changes:** startup stage, loading screen sections, the server connector's state, server address, protocol, relays and reconnect flags, the reconnect manager's state and retry count, iOS reachability, and which game windows are open.
  - **Network status:** Wi-Fi or cellular, IPv4/IPv6, and DNS available.
  - **The game's own console output.** Its logger is turned up to show everything for the first 2 minutes.
- **Run connection test** (shake menu, Help), which also runs automatically 3 s after launch. It runs:
  - DNS lookups with timing and addresses for the game's server (`s1.wannaplay.studio`), its API and a control site.
  - A direct connection to the game server's port 4055 over IPv4 and IPv6.
  - A web request to the game's API.
  This is to find out why loading gets stuck with NextDNS (DNS over HTTPS).

## [1.2.1] - 2026-10-01

### Fixed
- **Ball jumping over the pins** (1.2.0 bug). "Better spinning-pin hits" also put the ball on Unity's speculative collision checks. The ball spins at ~600 rpm, so those checks reached far ahead of it and made "ghost" contacts with the pin deck and pins that launched it into the air. The ball is back on the v1.1 collision checks for good.
- **Endless loading with NextDNS** (or any connection that neither works nor cleanly fails). The game only offers its gray "play offline" button once the connection gives up for good; otherwise it keeps reconnecting forever. Its startup wait for the privacy SDK also has no time limit. The new **Don't get stuck connecting** switch (on by default) adds the missing time limits: after 30 s stuck on "connecting", it shows the game's own offline button, and a privacy-SDK wait silent for 20 s (with no privacy page on screen) moves on, just like with no internet.

### Changed
- "Better spinning-pin hits" is now **"Spinning-pin hits (experimental)"**: pins only, **off by default**. It uses a new saved setting, so everyone starts on the v1.1 pin fix again.

## [1.2.0] - 2026-10-01

### Added
- **120 FPS mode** (Display & startup, off by default). The game picks its frame rate from its own table: 30 FPS for menus and 60 FPS for gameplay on phones (the PC / Facebook GameRoom build used 200). The tweak changes that table to 120, so the game applies 120 itself everywhere. The patched IPA now also sets `CADisableMinimumFrameDurationOnPhone = YES`; without it, iOS keeps every app at 60 Hz on iPhone. Uses more battery.
- **Auto-accept the privacy popup** (off by default). Presses "Sign up" for you on the My.Games privacy page that a sideloaded copy shows on every launch, by calling the page's own button function. It never presses pages about updated terms.
- **Better spinning-pin hits** (part of the pin fix, on by default). Pins and the ball use Unity's speculative collision checks, which also predict spin. The v1.1 sweep checks only follow straight-line motion, so a tumbling pin's top could end up inside the 10 pin before the hit was noticed, and the 10 pin barely moved. Turn it off to get the v1.1 behavior back.

### Fixed
- **Kicked back to the main menu after skipping the tutorial.** The game's own `SkipTutorial()` leaves the current tutorial stage behind; on your first throw that stage "finished" and restarted the lane. The tweak now cleans the stage up right after skipping, the same way the game does when a stage ends normally.

### Changed
- `VERIFIED_NOTES.md` now covers: the frame-rate table, the pin's real physics values (mass, balance point, inertia, friction, bounce), the privacy-popup flow, the bundled SDKs and network setup, and which fixes were confirmed on device (Match Up BP, Arsenal search, Skip tutorial button).

## [1.1.2] - 2026-10-01

### Fixed
- **Match Up BP skin.** The BP's store entry points at `black_pearl2_dif_x512.jpg`, a file that isn't in any of the game's asset bundles, so it never loaded. It showed plain white on the rack, or the last ball's skin in your hand. The real Black Pearl art does ship: it's the left half of `tex_blackpearl_flameturquoise.png`, the file the game had wrongly put on the Pearl. The BP now points there.
- **Skin file lookups.** v1.1.1 looked skin files up in `DLCManager.all_server_dlc`, which turned out to be empty on device ("not in catalog" for every ball). Now it uses `ItemsVisualData`, the catalog the game itself uses. It also checks the game's asset manifests to see whether a file really ships with the app.

### Added
- `VERIFIED_NOTES.md`: everything verified about the app (names, offsets, physics settings, how skins load, the tutorial, the Arsenal list), for whoever works on the tweak next.
- **Copy debug info** shows the catalog size and marks skin files that aren't in the app.

## [1.1.1] - 2026-10-01

### Fixed
- **Match Up Pearl skin is actually fixed now.** The Pearl's store entry points at the wrong skin file (the same black look as the Match Up BP). v1.1.0 saw that the Pearl had *a* skin and wrongly assumed the game had fixed it, so it changed nothing. Now the tweak points the Pearl at its real art: the left half of the Pearl/Hybrid skin file, checked by its file name in the game's skin catalog. The game then draws it burgundy everywhere: the rack, Arsenal, previews, your hand and replays.
- **Skip tutorial button got stuck on screen after skipping.** The game's `IsInTutorial()` keeps saying "yes" even after its own skip, because the skip never clears the active tutorial stage. Now the button hides the moment you tap Skip, and the game's "tutorial completed" flag keeps it hidden.

### Changed
- **One "Match Up skins" switch** instead of two. The "Real Match Up Pearl colors" switch is gone; the Pearl always gets its real burgundy art.
- **Match Up BP keeps the game's own black skin.** The drawn look-alike only shows up if the BP has no skin at all. Before that, a BP with no skin first gets the black skin the Pearl had.
- Turning the skin fix off puts the game's original skin links back right away.
- **Copy debug info** now lists the skin file each Match Up ball uses, and whether that file ships with the app.

## [1.1.0] - 2026-10-01

### Added
- **Auto-rack** for Spare shooting mode: pick your pins once, tap **Auto (every frame)**, and they get set up every frame without asking. Turn it off from the shake menu.
- **Skip tutorial** button during the first-launch tutorial. It runs the game's own built-in skip, so you can log in to your account right away.
- **Copy debug info** button in the menu, for bug reports.

### Changed
- **Ball speed** now goes from 1.0x to 5.0x in 0.1 steps (it used to go up to 500x). Saved values above 5x drop to 5x.
- **Pin physics fix:** pins can now spin up to 50 rad/s instead of Unity's old default of 7, so a pin clipped low at the base can tip over instead of just rocking in place. Only while the pin fix is on (Practice only).
- **Match Up BP look-alike** is checked every frame, so the wrong skin doesn't flash first.
- First try at fixing the Match Up Pearl in the game's data. It didn't work in testing, and was fixed for real in 1.1.1.

### Fixed
- **Arsenal search scrolling:** after a search, scrolling down showed empty space and the rest of the Arsenal disappeared. The game resizes the list with `ResetScroll()`, but v1.0 used `ReloadData()`, which only redraws what's already on screen.

## [1.0.1] - 2026-10-01

### Fixed
- **Game froze on the loading screen** and got closed by iOS. Unity 6 uses 64-bit handles for game objects, but the tweak stored them as 32-bit numbers, which broke the game while it was loading.

### Changed
- BowlingFix keeps its hands off for the first 5 seconds, then waits until the bowling lane has been up for a few seconds before doing anything.
- It never calls game code that creates things just by being asked. It reads the game's own saved values instead.

### Added
- **Safe mode:** if the game doesn't finish starting twice in a row, BowlingFix pauses itself so the game still works. A **Turn BowlingFix back on** button shows up in the menu.
- Version number at the bottom of the menu.

## [1.0.0] - 2026-10-01

First version.

### Added
- **Shake menu:** shake your phone to open it. A backup accelerometer detector catches shakes the game swallows.
- **Match Up skin fix:** the Match Up Pearl gets a skin from the game's own files instead of the last ball's skin or plain white, and the Match Up BP gets a drawn look-alike. (Both were reworked later.)
- **Pin physics fix:** pins and the ball use Unity's "Continuous Dynamic" collision checks, so fast pins can't pass through other pins. Practice only.
- **Ball speed** slider. Practice only.
- **Spare shooting mode:** a pin picker at the start of every frame, with presets (10 pin, 7 pin, 7-10, Bucket) plus All, None and Full rack. Practice only.
- **Arsenal search:** filter your Arsenal by ball name.
- The fun features only run in offline Practice and pause in online matches and tournaments.
- Settings are saved between launches.
- `tools/inject_ipa.py` to put the tweak inside an IPA, a GitHub Actions build, and a README.
