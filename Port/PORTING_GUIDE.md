# BowlingPlus: porting guide for the official game

For: the Bowling by Jason Belmonte team.
Covers: BowlingPlus 1.7.2 (GitHub `main`, commit `7304f4b`).

BowlingPlus is a tweak that patches the finished game at runtime. It reads the game's names and values from its IL2CPP metadata and changes them in memory. In the official Unity project most of these features are simpler: a component field, a ScriptableObject value, a material, a build flag, or a line of scoring code.

Start with `README.md` in this folder for the file list. The two features you asked about are in `features/120-fps.md` and `features/ipv6-loading-fix.md`.

## 1. How to read the table

- **Effort** is our estimate of the work in the official code: 1 = hours, 2 = a day or two, 3 = several days, 4 = a week or more with testing. The scale is for planning, not for estimating your schedule.
- **Gate** is where the feature should be allowed:
  - **Any**: visual or bug fix, safe in every mode.
  - **Practice**: only in offline Practice. Changes conditions or outcomes.
  - **Decision**: changes scores or physics for everyone, so the team decides per mode.
- **ID** is the stable name used in the compatibility section (section 4).

## 2. Features, easiest first

| # | ID | Feature | Effort | Gate | Notes |
|---|----|---------|--------|------|-------|
| 1 | `bp.fps120` | **120 FPS mode** | 1 | Any | Two frame-rate constants, one iOS `Info.plist` flag, one Android display-mode request. See `features/120-fps.md`. |
| 2 | `bp.ipv4` | **IPv6 loading fix**: game server over IPv4, and "don't get stuck connecting" | 1 for the watchdogs, 1-2 for the address rule | Any | The root fix is server-side (answer on IPv6) or one address-selection rule. The three loading time limits (30 s, 30 s, 20 s) port directly. See `features/ipv6-loading-fix.md`. |
| 3 | `bp.banner` | **Sweeper "BELMO" banner** | 1 | Any | The sweeper bar drew over both ends of the banner, and the banner dropped out near the back of the lane. The banner is a world-space canvas recessed inside the bar. Fix the draw order and placement in the sweeper prefab. |
| 4 | `bp.pinTurns` | **Pin turns kept through a re-rack** | 2 | Any | The game gives each pin a new random turn at every rack, so pins visibly turn when a ball is picked up. Keep the turn chosen at rack time. |
| 5 | `bp.bg` | **Alley background image** | 2 | Any | Your own picture behind the lanes (Fill, Fit, Stretch or Tile). Visual only. |
| 6 | `bp.pinLib` | **Pin texture library with a 3D preview** | 2 | Any | Swap the texture on the pin material, with a turning preview. Replays must keep the texture they were recorded with. |
| 7 | `bp.backup` | **Backup and restore of settings and patterns** | 2 | Any | Depends on your save system. |
| 8 | `bp.noTap` | **9-pin no-tap mode** | 2 official, 3 in BowlingPlus | Practice | 9 or more on a full rack counts as a strike, and the frame ends. In the official game this is a scoring rule for the mode. See section 3. |
| 9 | `bp.invisOil` | **Invisible oil** and its flash fix | 2 | Practice | The game's "show oil pattern" setting is 0 while the mode is on, so the oil is never drawn. Fixes a frame of oil showing after a ball is picked. |
| 10 | `bp.oilLib` | **Custom oil patterns**: library, PDF import, QR codes | 3 | Practice | Writes an oil map into `OilMapGenerator`. Real Kegel data sheets are parsed by `KegelParse.h`. Practice-only because patterns change difficulty. |
| 11 | `bp.oilBreak` | **Oil breakdown** (carry-down over shots) | 3 | Practice | Reloads the oil map each shot in Practice. |
| 12 | `bp.spare` | **Spare mode and pin picker** | 3 | Practice | Choose which pins stand for a spare shot. |
| 13 | `bp.speed` | **Ball speed and spin tools** | 3 | Practice | Scales the ball's speed once, at launch. Practice only. |
| 14 | `bp.replayRate` | **Replays at the recorded rate** | 1 in the official game | Any | Only needed if the physics step changes. BowlingPlus's replays played at half speed after the double rate (fixed in 1.7.2). An official build that changes the step should record and play back at the same step. |
| 15 | `bp.pinFric` | **Realistic pin physics** (pin friction 0.25) | 2 | Decision | Friction on the pin materials. On the Bowlscore grid the model strikes about 35-37 % at 0.25 (at the game's step), against about 25-27 % for the game as shipped and 41.9-44.3 % for real pins (USBC). |
| 16 | `bp.doubleRate` | **Double physics rate** (3.75 ms step) | 3 | Decision | Halves the fixed timestep. Changes how often oil, friction and replays are updated, so it needs the replay check in row 14. In the model it strikes about 40 % on the Bowlscore grid (rerun 40.4 %, study 40.5 %). |

Not worth porting, or better rebuilt in the official code:

- **The DNS import hook** (`src/Dns.mm`, `android/native/Dns.cpp`). It exists only because we can't edit the game's code. Use the server or address fix instead.
- **Auto-accept on the privacy page.** We don't recommend porting it. It presses "Sign up" for the player on the privacy page.
- **The in-app log and "Copy debug info".** Useful for your own QA on phones, and easy to port if you want them.

## 3. Practice gating

BowlingPlus changes gameplay only in offline Practice, using one flag:

```
gBFStatus.offline = (mode == MODE_FUN) && (loc != LOC_REPLAYER)
```

- The game's modes, as BowlingPlus sees them: `MODE_FUN = 0` (Practice), `MODE_COMPETE = 2`, `MODE_NONE = 4`, `MODE_TUTORIAL = 5`.
- Every gameplay-changing feature (rows 8-13 and 15-16) checks this flag. Oil features also count the shot replayer as Practice, so a replayed shot shows the same oil as the shot it replays. Tutorial mode is always excluded.
- When the player leaves Practice, each feature restores the game's own values. Settings are saved but inactive outside Practice.
- Cosmetic features and bug fixes (rows 1-7 and 14) are outside the gate, so every mode gets them.

For the official game, we suggest copying the idea, not our code:

1. One check, `PracticeFeatures.IsAllowed(gameMode)`, that is true only for Practice.
2. Gameplay features read their value through that check. Competitive, online, league and tournament modes always see stock values.
3. Keep cosmetic and bug-fix features outside the check.
4. Mark replays with the settings they were recorded under, or don't allow practice-modified shots into shareable replays.
5. Rows 15 and 16 are Decision items. A safe path: Practice first, then a separate "realistic" ruleset with its own leaderboards.

## 4. Keeping BowlingPlus and the official fixes from applying twice

BowlingPlus can't tell whether the official game already has a fix. Today each feature is written to be harmless if the official version has it, with one exception noted below.

| ID | If the official game has it | What BowlingPlus does today |
|----|-----------------------------|-----------------------------|
| `bp.fps120` | Official setting sets the same constants | Reads the current values, writes 120, and restores what it found when off. Harmless. Hide BowlingPlus's switch. |
| `bp.ipv4` | Official client picks IPv4, or the servers answer IPv6 | The filter returns IPv4 either way. If the import slot is gone it does nothing (`dnsSlots=0` in the log). Hide the switches. |
| `bp.pinFric` | Official materials already use 0.25 | Sets 0.25 on the materials and restores what it found when off. Harmless. Hide the switch. |
| `bp.doubleRate` | **Known gap.** See below. | Halves any engine step between 2 and 50 ms. |

**The known gap:** `bp.doubleRate` halves whatever step it finds, not specifically the game's 7.5 ms. If the official game ever ships a different step, BowlingPlus would halve that too. Until BowlingPlus checks for exactly 7.5 ms, don't run both. We haven't changed this in a release yet.

**What we'd like from you:** the IDs you ship, and when. Then the next BowlingPlus release can stand down on any build that already has a feature. We'd check the game version in the app's bundle and skip those features. This isn't built yet.

## 5. What's in this folder and how far it's been tested

- `features/120-fps.md`: how the frame rate is chosen and set, iOS and Android code, status.
- `features/ipv6-loading-fix.md`: the cause, both platform implementations, the three watchdogs, the official fix options, and a test plan.
- `pin-physics/`: the study, the exact physics settings, the Bowlscore reference and what the two percentages measure, the engine-step notes for the double rate, the PhysX harness (`pinlab/`), and the Bowlscore runs with a summary script (`results/`).

Testing status, honestly:

- **120 FPS:** the iOS flag was confirmed on a phone (BowlingPlus 1.6.1). Smoothness has only been reported by users. Android compiles and passes CI; no phone test is recorded.
- **IPv6 fix:** confirmed on an iPhone after 1.2.3 with NextDNS. The Android port compiles and passes CI; no phone test is recorded.
- **Pin physics:** the percentages come from our PhysX 4.1 model of the game, not from the game itself. A few phone games on the double rate were played; that's too few to measure a strike rate.
- **Everything else:** compiled and checked by our simulators where a simulator exists. Not yet tested on a phone: 9-pin no-tap, the replay-rate fix and the invisible-oil fix (1.7.2), and the library row layout (1.7.1).

## 6. Don't sign or share keys

Do not share BowlingPlus's Android signing key or its password. The team should sign its own builds.
