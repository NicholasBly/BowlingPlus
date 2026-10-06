# Changelog

Every change to BowlingFix, newest first.

## [1.6.4] - 2026-10-06

### Fixed
- **Pins still turned every time you picked up a ball.** 1.6.2 and 1.6.3 watched the wrong pins. The game has two pin systems, `PinHolder` and the older `InventaryData.kegels` (it picks one with `InventaryData.UsePinHolder`), and on this game the PinHolder pins are a set that never moves: Copy debug info showed them with no turn on any pickup. The pins on the lane are the kegels, and those are now watched too (watching both is harmless: a pin that never turned has nothing to put back). A simulation of exactly that (kegels on the lane, PinHolder pins still) reproduced the 1.6.3 debug line character for character; the new code passes all 12 checks there, including repeated pickups and pickups before a spare shot.
- **Picking up a ball counted as a throw** (Copy debug info: `ball=3` with no throws), because the game passes through its "throwing" location when you pick one up. A throw now means the ball was actually launched (moving faster than 1 m/s), the same test the speed boost uses.

### Added
- **Bowl on the other lane (experimental)**, in Practice fun. The game still has two lanes: dragging your shoes at the ball rack moves you over, but it snaps back when you let go. With this on, BowlingPlus asks the game to move you (its own `RoadChanger.shiftToLaneNumber`) while you're at the ball rack; turn it off to go back. If the game keeps moving you back, BowlingPlus stops after 5 tries and Copy debug info says `GAVE UP`. How the other lane plays (its oil and pins) isn't checked yet: try it and send Copy debug info.

### Changed
- **Copy debug info:** `pin turns:` now lists both pin sets (`holderPins`, `kegels` and the kegels' type), real throws, and which set turned between checks. `lanes:` adds `UsePinHolder` and the other-lane state (`home`, `now`, `tries`, `reverts`).

### Not tested on a device when this was written
All of it. The pin-turn and lane code are identical on iOS and Android; both were run against simulated games (the lane one also with a game that keeps snapping back), and Android compiles and links.

## [1.6.3] - 2026-10-06

### Fixed
- **Pins still turned every time you picked up a ball** (1.6.2's fix never kicked in: Copy debug info showed `kept=0 knocked=0` after 4 throws). Two reasons, both reproduced in a simulation that then gave the same numbers:
  - **It only accepted a pin leaning less than 1 degree from its saved pose.** A pin at rest on the deck leans a little, and a fresh rack stands it exactly upright, so the difference was over 1 degree and nothing was put back. Now up to 6 degrees counts as standing (15+ is knocked), and only the turn is undone: the pin keeps whatever lean it has, so this can't move it in any other way.
  - **Saved turns were thrown away about once a second.** They were kept by pin-holder slot, and the holders were looked up again every 120 frames (1 s at 120 FPS) in whatever order Unity returned them, which also hid every pin that fell during a throw. They're now kept per pin.
- **Practice could still be reached before the oil, colors and ball fixes.** 1.6.2 started 1.5 s after the game's main menu came up; now it starts the moment it's up (the main menu is when loading is finished, and it's the first screen Practice can be reached from). The plain 4 s wait is only used if the main menu can't be seen.

### Changed
- **Copy debug info:** a `pin turns:` line (pin holders, pins read, read failures, turns put back, new frames, the biggest lean seen during a throw, and how often a pin's physics body or its visible model turned between checks), so if pins ever turn again it shows where. The `lanes:` line now reads static fields from the right place (1.6.2 showed `currentLane=475208576`, which was the object's header), lists `shiftBy`, and checks that `shiftToLaneNumber` takes a whole number.

### Not tested on a device when this was written
All of it. The pin-turn code (identical on iOS and Android) was run against a simulated lane with settling pins and pin holders returned in a different order every time; the old code gave your device's numbers there, the new code passes all 9 checks. Android compiles and links.

## [1.6.2] - 2026-10-06

### Fixed
- **Pins turned to a new random angle every time you picked up a ball** (easy to see with your own pin image). The game's pinsetter gives each pin a random turn whenever it racks, and the game racks again for things that aren't a new frame: picking up or switching a ball, a spare-mode pick. BowlingPlus now remembers each pin's turn when a frame is racked and puts it back on every re-rack until the next frame. A new frame is a full rack after the frame's 2nd ball or after a pin went down (a strike); a full rack after a 1st-ball miss is the same frame, and pins left standing for the 2nd ball keep their turns too. Practice only; only standing pins in their spot are touched, never during a throw or replay. (Pins are round, so their turn doesn't change how they play.)
- **A thin line of oil, in the oil color, at the very back of the lane behind the pins.** The oil mirror fix needs the oil picture to wrap across the lane, and this game only has Unity's one wrap setting for both directions (the across-only and along-only setters are stripped from it), so the far end of the lane also wrapped around to the heavy oil at the foul line for the last half row. The picture is now stretched a hair along the lane (0.25% for the game's 240-row maps, the drawing ending about an inch early at 40 ft) so the far end stops inside the last row. Looks only: the oil you bowl on is unchanged.
- **Right after the game started you could reach Practice before the Match Up ball fix, the oil color and custom oil were on.** BowlingPlus waited a fixed 4 s after the lane appeared before doing anything. It now starts 1.5 s after the game's main menu is up (its startup is done then), and keeps the 4 s wait only if it can't see the main menu. Copy log shows when it settled.

### Added
- **Copy debug info: a `lanes:` line.** The game still has its two-lane system (bowling on the lane to your right by dragging your shoes): `RoadChanger` (current lane, number of lanes, a one-lane switch, how far the lanes are apart, and a "move to lane N" function), the shoe-drag gesture `SwitchLaneDrag`, and a `CHANGE_LANE` game setting. This line reports what the game has live, with each value's real type, so lane switching (and alternating lanes with their own oil) can be built on facts. It changes nothing. The same line also shows the pin-turn and oil-edge fixes at work.

### Not tested on a device when this was written
All of it. The pin-turn logic was run against a simulated lane in 9 scenarios (pickups, a gutter 1st ball, strikes, spares, pins left for the 2nd ball, online), the oil-edge margin was checked against how the GPU picks rows for 60 to 480-row maps, and the Android build compiles and links. iOS can't be compiled where this was written; it was checked for the declaration-order mistake that broke the 1.6.1 iOS build.

## [1.6.1] - 2026-10-05

Android fixes, and the iOS build fix.

### Fixed (iOS)
- **The iOS build failed** (`src/MenuButton.mm`: "use of undeclared identifier 'BFMenuButtonClampedCenter'" and "'BFMenuButtonSavePosition'"). The on-screen menu button's touch code called two helpers defined further down the file; Objective-C++ needs a function declared before its first use. They are declared at the top now. This had broken every iOS build since the button and Backup were added after 1.6.0, so **the iOS on-screen menu button and Back up my data appear for the first time in this version.**
- **The iOS menu button could end up invisible.** It went into whichever window was "key" at startup, and every launch the game shows its privacy page, which BowlingPlus hides by making the SDK's own window invisible; a button put there was invisible too, and gone once that window closed (until a shake opened and closed the menu). It now lives in the game's own window (the same one the privacy code treats as the game), looks again whenever the app comes to the front, and only comes to the front when it is added, so a panel opened over it (the oil library) stays on top.

### Fixed (Android)
- **The Android build failed to compile.** `BP.java` called `FbLogin.inspectIntent`, which didn't exist. It does now: it logs what Facebook's browser login sends back (that a token arrived, or Facebook's own error text such as "Invalid key hash"; the token itself is never logged), once per redirect, and can't throw out of a lifecycle callback.
- **The menu scrolled at a crawl.** While any BowlingPlus panel is open the lane behind it isn't being played, but the game kept drawing it at 60 or 120 FPS, which took the GPU the panel's scrolling needs. The game is now capped to its own 30 FPS menu rate while a panel is open and put back to what it was (or 120 if 120 FPS mode is on) the moment the last one closes. Also: the menu had two scroll containers inside each other (the inner one never scrolled but sat in every touch), and its twice-a-second refresh re-set labels whose text hadn't changed, each of which can re-measure the whole menu. Both are gone. "Copy debug info" has a new `panelCap=` value on the `fps:` line (the rate the game goes back to, or -1 when not capped).
- **The Custom oil library and the pin picker were pushed against the left edge.** Cards that gave only a vertical position got no horizontal one; every card is now centered.
- **The footer's first line (logo, BowlingPlus, Donate) sat at the left** while "Check for updates" and the version were centered. All centered now.
- **The on-screen menu button drew on top of panels** (it covered the "Custom oil" title). It now steps aside while any panel is open.
- **"On-screen menu button" and "Browser Facebook login" didn't stick.** The native side didn't know either setting, so they weren't saved and the menu's refresh reset them (the button switched itself back off). Both are saved now.
- **Screen freezes.** The UI no longer waits on the game for: every step of the oil-color slider and the pin/spare/arsenal/skip commands, the pattern editor's preview, "Start from" list and first load, Copy debug info / Copy log, and reading a picked photo or file. A late "Start from" answer can't overwrite steps you've already edited.
- **Settings file rewritten on every slider step.** It is written once, 300 ms after you stop.
- **Privacy page.** It could hide the whole game when shown over it (the hide climbed past the screen's content frame); it now stops there and never touches Unity's own view, and only MRGS's own page is hidden on sight (other web views stay visible until confirmed). First time, it is remembered only if you pressed Sign up; closing the page any other way is logged and it shows again.
- **Memory protection after the DNS patch** is put back exactly as it was instead of always read-only.
- **Crash guards:** every activity lifecycle callback is wrapped; the start-up code can't run twice; each JNI method lookup checks for an exception before the next call.
- **Kegel PDF import** deleted its temporary copy only when it worked; it now always does. **Backup** keeps only the newest earlier zip instead of piling up 40 MB files.
- **"Check for updates"** said it couldn't reach GitHub when the repo simply has no release yet; it now says so.

### Changed
- **The iOS workflow can build the patched IPA** (`BowlingPlus-<version>.ipa`) when it has a link to the game IPA (secret `GAME_IPA_URL`, or the `ipa_url` box), using `tools/inject_ipa.py`, and checks the result has the 120 Hz setting. Only the patched IPA can run above 60 Hz: the game's Info.plist sets `CADisableMinimumFrameDurationOnPhone` to NO, and injecting the dylib with Sideloadly's own option (or installing the .deb) leaves it that way, so 120 FPS mode then runs at 60 (`plist120=0` in Copy debug info). The README's Sideloadly inject option now says so.
- The Backup card now warns that the file contains a logged-in Facebook session and should be kept private.
- The Android README's Facebook section matches the code: the browser login's result on a re-signed build is not confirmed yet, and Copy log now shows an `[fb]` line with what Facebook sent back.

### Not tested on a device when this was written
iOS: the on-screen menu button and Back up my data have never run on an iPhone (they could not be built until now); the fixes above were checked by reading the code, not compiled here (no iOS SDK in the environment), so the CI build is the first compile. Android: the 30 FPS cap while panels are open (its logic was run against a fake game in 7 scenarios, including turning 120 FPS on or off while a panel is open, but the speed-up itself is unmeasured), the centered cards and footer, the button hiding under panels, the Sign-up detection (it needs the WebView to report the page's title, and if it doesn't, the page just keeps showing as before), and signing in `patch_apk.py`.

## [1.6.0] - 2026-10-04

### Added
- **Tap the pin layouts to pick pins** (Practice). Tap the pin layout in the top right while you hold the ball, or the little screen under the ball return in the overhead view, and the pin picker opens for that one shot, starting from the pins that are standing. Rack 'em sets them up right away and the pick lasts until you throw (it survives switching balls); the next frame is a normal rack. Spare shooting mode in the menu is still the way to get it every frame.
- **An X on the pin picker.** It closes the picker without changing anything, in both modes. In Spare mode it also won't ask again that frame.
- **The game's own 48 oil patterns are now drawn like real life in Practice.** Each pattern carries its real Kegel file; BowlingPlus draws the lane from it with the Kegel-accurate model (exact distances, microliters, reverse oil adding up, brush carry-down, brushed film over the whole lane, Kegel's left on the bowler's left) instead of the game's simplified engine. All 48 are switched at once so the pattern carousel shows them right away, and they go back to the game's own the moment you're not in Practice (online matches, tournaments and the tutorial always use the game's oil). Checked on all 48 files: no gaps in the film, nothing past the pattern's distance, and total oil between 0.72x and 1.19x of what the game's engine makes from the same file.
- **"Start from" in the pattern editor** copies a game pattern's real file data (microliters, exact distances, drop brush, length).
- **`.txt` pattern import** also reads the second file layout the game uses for some patterns.

### Changed
- **Shake menu redesign.** Grouped into cards (Arsenal search, Practice fun, Oil, Fixes, Pins & display, Help & diagnostics), each foldable by tapping its title. Descriptions are hidden: the ⓘ in the header shows all of them, tapping a row's title shows just that one, and your choice is remembered.
- **Privacy popup: accept it once, never see it again.** The menu option is gone. The first time, you accept the My.Games page yourself; BowlingPlus notices it went away and remembers. From then on the page is hidden as soon as it appears and its Sign up button is pressed for you. Pages that aren't the first-time Sign-up page (like updated terms) are shown normally and never pressed, and a page that doesn't load within 8 seconds is shown again. If auto-accept was on before, it counts as already accepted.
- **Kegel-accurate oil is no longer a setting.** It is always used for custom and imported patterns (and now the game's, in Practice).

### Fixed
- **"Check for updates" was cut off** ("Check...pdates") on narrower layouts. The footer is now two lines: logo, BowlingPlus and Donate on the first, Check for updates on its own line.

### Not tested on a device when this was written
The menu layout, hiding the privacy page, and the pin-layout tap detection were built and compiled, but not seen on a phone. "Copy debug info" now has a `pin tap:` line (what the last tap hit) and a `privacy:` line to help if anything misbehaves.

## [1.5.3] - 2026-10-03

### Fixed
- **Custom patterns now match Kegel's charts.** Compared cell by cell with the charts of four patterns (23,088 cells), the oil lands where the sheet says in 99.90% of them; the rest are the arrow triangles drawn on the chart. Causes of the old mismatch (found on the 2025 U.S. Open #4):
  - **Rounded rows.** Steps were placed on whole-foot rows from the sheet's rounded numbers, so neighboring left and right rectangles overlapped by a full foot. Kegel's sheet numbers are rounded for display ("10→14" is really 9.80→13.72, because each load travels speed × 0.14 ft). Oil is now placed on the lane's real quarter-foot rows with exact distances. Only the true overlaps remain.
  - **Bare boards.** The game's oil engine only carries oil on boards the oil head crossed, so boards 6–7 and 33–34 had nothing (zero oil between the arrows). Kegel's chart has a brushed film over every board out to the pattern distance, in two tiers split at the reverse brush drop. Now every board from 2 to 38 has it.
  - **Inflated oil.** 1.5.2 rescaled each pattern to the game's own total, which made every pass up to 1.8× too heavy. Oil is now in the game's own single-pass units, with no rescaling.
- **Import accuracy:** end distances are recomputed from loads and speeds when a sheet only has rounded numbers (PDFs, text), and used as they are from Kegel's `.Pattern` files and the collection.

### Added
- **`tools/oil_verify/`:** the script that checks the oil model against Kegel's charts.

## [1.5.2] - 2026-10-03

### Fixed
- **Kegel-accurate oil left the front of the lane too thin.** After its last reverse oil load, a lane machine travels back to the foul line with the brush still down, and that brush wipes oil onto the front of the lane. The sheet lists that step as 0 oil because the pump doesn't fire, not because no oil lands. 1.5.1 treated it as laying no oil; it now carries the oil down like the machine does. 2017 SEA Games Long now looks like its sheet: heavy front, darkest center at 10–19 ft with lighter outside boards, the pyramid, and light outside past 19 ft.

## [1.5.1] - 2026-10-03

### Added
- **Kegel-accurate oil** (Oil section, on by default). Custom patterns lay oil the way the sheet describes it instead of the game's simplified way:
  - each step's microliters (the MICS column) count
  - reverse oil adds on top of forward oil instead of doubling it
  - travel steps (0 loads) lay no oil
  - past the oiled area, only a thin brushed film carries forward

  The total amount of oil stays the same as the game's own model, so patterns aren't drier or slicker overall; it's just placed like the sheet. Patterns like 2017 SEA Games Long now show several oil levels on the lane instead of two.
- **Microliters per step:** imported from PDFs (MICS), `.Pattern` files and `.txt` headers, editable in the pattern editor ("MICS"), and kept in share codes.
- **Pin picture preview for your PC** (`pins/BowlingPlus-pin-preview.html`). Drop in a picture and see it on a spinning 3D pin, processed exactly like BowlingPlus does. You can drag to spin and tilt, right-drag to move, zoom, use quick views (Side A / Side B / seams), download the processed picture, and download the wrap template and guide.
- **`PIN_TEXTURE_SYSTEM.md`:** how the pin picture system works, for whoever works on it next.

### Note
- A Kegel sheet's lane chart shades the lane by which pass oiled it (forward, reverse-only, brushed), not by how thick the oil is, so its colors won't line up exactly with the lane's thickness colors.

## [1.5.0] - 2026-10-03

### Fixed
- **The RPM boost didn't add hook.** The game works out the ball's grip on the lane from its spin, but caps the spin at the ball's maximum, so extra revs past that did nothing. Boosted throws now also get the extra grip the uncapped formula would give, so more revs means more hook. It's only for that throw, and the game's value comes back right after.
  - Very fast spin can look slow or even backwards on screen (like car wheels in videos). That's the frame rate, not the physics.

### Changed
- **Pin pictures:** the wrap sheet is now the way to design a pin: one piece that wraps around the pin like paper, so there are no seams. "Get the wrap template + guide" gives the wrap template and its guide. Square pictures in the game's layout still work.

## [1.4.9] - 2026-10-03

### Added
- **Ball spin (RPM) boost** (Practice fun, under Ball speed). Multiplies your throw's spin right after release, 1x to 17x. The game's controls cap spin near 600 rpm; 17x takes that to about 10,000. The game's physics allows far more (100,000 rad/s), so nothing gets cut off.
  - **Same spin direction:** the hook side stays the same.
  - **More spin = more hook**, worked out by the game's own lane friction and oil.
  - **The scoreboard** shows the boosted rpm.
  - **Practice only**, like the speed boost.

## [1.4.8] - 2026-10-03

### Added
- **Wrap layout for pin pictures.** A 2:1 picture that's the pin's surface unrolled evenly: left to right once around the pin (the edges meet), top to bottom from head to base.
  - **Converted on your phone:** BowlingPlus maps it onto the game's layout using the pin's actual 3D shape, so seams always match, and evenly spaced designs (like a band of spikes) stay even all the way around.
  - **Guides:** "Get the layout guide" now also gives a wrap guide and a wrap template (the game's pin unrolled).
  - **Square pictures** still use the game's own layout.

### Fixed
- **A crack near the base** with hand-drawn pictures whose pin outline sits a little inside the real one. Background-colored slivers near the shape edges, and the blended pixels right at the edges, are now refilled from inside. Your picture is reprocessed automatically from now on when processing improves (BowlingPlus keeps the original).
- **The "Game server over IPv4" fix was never actually switched on.** The game loads its main code slightly after BowlingPlus starts, so the fix found nothing to attach to (debug info: `dnsSlots=0`). It now attaches as soon as the game's code loads.
- **Safe mode paused BowlingPlus for players whose game was just stuck loading** (for example, when the game's server can't be reached). Only crashes count now: a launch still running after 60 seconds clears the counter, so "Fix connection" can offer the offline button.

### Removed
- **"Pins face random ways"** (1.4.5). The game already shows each pin's random turn. Its standard pins just look the same from every side. BowlingPlus's version never activated.

## [1.4.7] - 2026-10-02

### Fixed
- **Your pin image popped in at the start of every frame.** The pinsetter lowers its own set of pin models (plus pin-deck and reflection copies), and those still showed the game's pins. They now show your image too, and BowlingPlus re-checks every frame instead of every half second.
- **A crack down the side of the pins**, where the two halves of the picture meet. The GPU blends in whatever is just outside the pin shapes, so a picture with another color or transparency there showed a line. BowlingPlus now fills the area around the shapes with the colors at their edges, and makes the picture fully solid like the game's. Your current picture gets this automatically on the first launch.

## [1.4.6] - 2026-10-02

### Added
- **Use your own pin image** (new "Pin look" section in the shake menu):
  - **Picking:** choose a picture from Photos or Files and it's wrapped onto the pins on every lane, reflections included. A non-square picture is stretched to square so it lines up.
  - **The layout guide:** "Get the layout guide" saves or sends a guide showing exactly where each part of the picture lands (head, neck, body, base, the two sides and the underside), plus a clean template of the game's own pin to paint over.
  - **Switching:** turning it off brings the game's pins back. If you change pins in the Pin Arsenal, your image stays on.
  - **Looks only.**

## [1.4.5] - 2026-10-02

### Added
- **Pins face random ways** (Display, on by default). The pinsetter already sets every pin turned a random way, but the game always drew them facing the same way. BowlingPlus shows each pin's real turn, so racks look like real life, and flying pins now visibly spin around their own axis. Works on the neighboring lanes and in the lane reflection too. Looks only: the physics is exactly the game's.

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
