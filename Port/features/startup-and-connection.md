# Startup and connection

These improvements need the game's source, so they're written for the official project. BowlingPlus doesn't change startup.
Gate: **Any**. None of this changes gameplay.

The sequence was read from the 1.907 Android build; exact addresses and offsets are in `VERIFIED_NOTES.md`, section 7c, in the BowlingPlus repository. The timings come from one console log on an iPhone 17 Pro (iOS 27.0.1, sideloaded copy, Wi-Fi), below. The game's own `ClientAnalitics.LoadingTime` events and the Unity Profiler will show it across real players.

## Measured on an iPhone 17 Pro

| Seconds since launch | What happened |
|---|---|
| about 1.3 | The privacy page appears (the consent step) |
| ? | SDKs, then assets: `LoadingTime_InitResourceSystem` 0.936 s, `LoadingTime_BundlesLoaded` 1.432 s |
| ? | `Game3` loads and switches on: `LoadingTime_SceneLoaded` 0.936 s |
| about 4.3 | The game looks up its server: connecting starts |
| about 4.97 | `CONFIG LOADING TIME: 0.642`, then 83 store products |
| 5.58 | Logged in, `cc_main_menu` |

The connection screen (Wi-Fi bars) lasted about 1.3 s here, which is roughly what item 1 below would save on this phone. The steps before 4.3 s couldn't be timed exactly: the game prints each store product id dozens of times at about 5 s, which pushed the earlier console lines out of the capture.

## What the player sees, and why

| Bar shows | What the game is doing |
|---|---|
| **0 %** | `InitState.InitAfterSceneLoaded`, one step after another: Unity Services (waits with no time limit), IAP, Firebase, AppsFlyer, migration, the CDN test, IronSource pre-init, MRGS, the privacy-consent callback (no time limit), IronSource init, notifications |
| **0 → about 40 %** | The resource system loading asset bundles (48 MB ship in the APK) |
| **about 40 → 84 %** | `SceneManager.LoadSceneAsync` loading `Game3` |
| **stuck at 84 %** | Unity switching `Game3` on: 7,969 GameObjects and 7,310 scripts run `Awake`, `OnEnable` and `Start` on the main thread, and the Loading scene is unloaded |
| **Wi-Fi bars screen** | Only now does the connection start: `MainMenuState.OnEnter` wakes the `Connector` (which lives in `Game3`), then master server, redirect, game server, config request, authentication check, login, main menu |

Why the bar stops at 84 %: `Loading.FixedUpdate` shows `(r + p × (1 − r)) × 90`, where `r` (asset progress) stops at 0.45 and `p` is the scene's `AsyncOperation.progress`. Unity holds `progress` at 0.9 while it activates the new scene, which gives about 84–85 %. The Loading scene is destroyed when `Game3` takes over, so the bar never reaches 100 %.

## Native improvements, biggest first

### 1. Start connecting during the loading bar

- The `Connector` is already `DontDestroyOnLoad`. Create it, with the connection sequence (`ClientSideManager.startConnectionSequence`, `ConfigManager.Init`), in the Loading scene or from `InitState`, instead of in `Game3` and `MainMenuState.OnEnter`.
- Then the connection, config request and login overlap the asset and scene loading. If they finish first, the Wi-Fi bars screen disappears.
- **Watch out:** `LoadingWindow` currently waits for connection and login events (`HandleServerConnecting`, `HandleConnectionCompleted`, `HandleAllConfigParsed`, `OnPlayerLoggedIn`). If those fire before `Game3` exists, the window must read the current state when it opens instead of waiting for an event that already happened.
- **Watch out:** `Connector.OnConnectorAwake` also sets up the avatar and replay managers on its GameObject. Move those with it, or keep them in `Game3` and attach them when it loads.

### 2. Split `Game3`

- It's one scene with the whole game. Load the main menu first, and bring in the shop, arsenal, tournaments, replays and other screens as additive scenes or prefabs when they're opened (`SceneManager.LoadSceneAsync(name, LoadSceneMode.Additive)`, or Addressables).
- Anything that isn't shown at startup should start inactive: inactive objects don't run `Awake` or `OnEnable` until they're switched on. They're still loaded, though, so splitting saves more.
- This is what removes the 84 % stall.

### 3. Don't make the game wait for the SDKs

- Start Unity Services, IAP, Firebase, AppsFlyer, IronSource and notifications together, and don't hold the scene load on them. Only the consent step has to come before the ad and analytics SDKs; the game itself doesn't need to wait for it.
- Give `UnityServices.InitializeAsync` and the privacy callback time limits (for example 15–20 s), then carry on as if offline. Today both can wait forever. A player reported the App Store version stuck at 0 %.

### 4. Load cosmetic bundles when they're needed

- Balls, shoes, pins and gloves don't all need to be loaded for the main menu. `ResourceBundleHolder.LoadAllBundles` exists; check with the Profiler whether startup runs it, and if so load bundles when a screen needs them.

### 5. Smaller wins

- Set `Application.backgroundLoadingPriority = ThreadPriority.High` during the loading screen, and back to normal afterwards. Async loading gets more time per frame, and a choppier loading screen doesn't matter. (This setting is stripped from the shipped build, so BowlingPlus can't do it.)
- Move the loading label update from `Loading.FixedUpdate` to `Update`. It formats a string every physics step (about 133 times a second) while loading.

### 6. The error reporter has never worked

- `CloudReporter` hooks `Application.logMessageReceivedThreaded`. Every log message from every thread passes through it, and errors, asserts and exceptions are queued under a lock. Each frame it builds a report and starts an upload to `https://clickhouse.wannaplay.studio/error/`.
- On the device, every report fails with "Malformed URL" before it reaches the network: the stack trace goes into the URL without full escaping, and the `log` parameter has no `=` (`&logNullReferenceException...`). The host also doesn't exist (no DNS record). So no report has been delivered.
- Fix the URL (escape it, or send the report in a POST body) and the host, or remove the reporter. If you keep it, batch the uploads and limit how many are sent per session.

### 7. Other things in the startup log

- **The password hash is printed on every launch:** "Logged in success user id ... psw <hash> saved psw <hash> HASH <hash>". Remove that line from release builds.
- **The login handler throws:** a `NullReferenceException` in `Facebook.Unity.FacebookBase.API`, from `FacebookManager.RequestFriends` <- `ClientSideManager.placeListenersOnLogin` <- `HandlePlayerLoginSuccessfully`. Whatever comes after `RequestFriends` in `placeListenersOnLogin` doesn't run. It was seen on a sideloaded copy (Facebook may not have initialized there); check the App Store build.
- **Store products are logged dozens of times each** at startup (83 products). Cheap, but it buries the rest of the log.
- **A missing ball texture triggers a download attempt at startup:** skin 413 (`black_pearl2_dif_x512.jpg`) isn't in the app, so the game tries the cache, the app's files and a download (404) while the ball return fills.

## What doesn't help much

- **More cores.** Photon already resolves and connects on its own background thread, and Unity already streams scenes and assets on worker threads. What's slow is main-thread work (activating `Game3`, SDK start-up callbacks), and Unity's API has to run on the main thread. The gains come from doing less work up front and not waiting on things one at a time.

## Measuring it

- **Your analytics:** `ClientAnalitics.LoadingTime` is sent after the resource system and after the bundles load.
- **Unity Profiler:** on a device, the frame where `Game3` activates shows the 84 % stall directly.
