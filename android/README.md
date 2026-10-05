# BowlingPlus for Android

An Android port of BowlingPlus (the iOS tweak in this repo) for *Bowling by Jason Belmonte*
(`studio.wannaplay.bowlingjb`, Unity 6000.0.67f1, IL2CPP, arm64). It adds the same shake‑to‑open menu and the
same features as the iOS build: ball speed / spin, the pin‑physics fix, spare shooting, skip‑tutorial, the
connection fixes, 120 FPS, Arsenal search, the whole oil system (Kegel‑accurate patterns, the custom‑pattern
library with QR sharing and Kegel import, the color picker, invisible / breakdown / thickness), the Match Up
skin fix, and your own pin image.

Everything about *how a feature works in the game* is the same C++ as the iOS version — the two builds share
`src/` (the game logic is `src/Game.mm`, ported to `android/native/Game.cpp`). Only the parts that used
Foundation / UIKit were rewritten for Android.

## How it works

The game's `lib/arm64-v8a/libmain.so` is the small Unity bootstrap that Java loads first. The patched APK
renames it to `libmain_orig.so` and ships **our** `libmain.so` in its place. Our `JNI_OnLoad`
(`android/native/Jni.cpp`):

1. `dlopen`s `libmain_orig.so` and forwards `JNI_OnLoad`, so Unity starts exactly as before;
2. starts the IPv4 DNS filter (it rebinds `getaddrinfo` in `libil2cpp.so`, the same data-only trick as
   fishhook on iOS);
3. starts the BowlingPlus Java side (the shake menu and the oil / pin UI). Its classes are added to the APK as
   one extra `classesN.dex`, so the game's own class loader finds them like any other app class. That is also
   what lets Android create the two small components the menu needs (below).

The per-frame work runs on Unity's own thread: once per screen refresh, Java posts a job with
`UnityPlayer.invokeOnMainThread(Runnable)` (never more than one waiting), which Unity runs between frames (the
Android version of the iOS `CADisplayLink`). Menu actions that touch the game are queued onto that same thread.
Taps on the pin layouts are watched (never consumed) through the activity's `Window.Callback`.

None of the game's own code, resources or libraries is modified. The manifest gets a transparent helper
`<activity>` (for the photo / file pickers), a small read-only `<provider>` (for sharing QR codes and the pin
template), and loses the "this app comes in splits" markers, since the patched APK is one file.

## Layout

```
android/
  build.sh    the whole build: Java -> bowlingplus.dex, C++ -> libmain.so, then patch the game
  java/       the shake menu and oil / pin UI (package com.bowlingplus)
  native/     the tweak in C++ (shares src/ with the iOS build) -> libmain.so
  tools/      patch_apk.py (merge, swap, add the dex, edit the manifest, sign), axml.py (binary XML)
.github/workflows/build-android.yml
```

Native files: `Jni.cpp` (entry point, thread bridge), `Game.cpp` (the ported game logic), `Il2Cpp.cpp`
(by-name IL2CPP lookups), `Engine.cpp` (settings + crash guard), `Log.cpp` (event log + connection test),
`Dns.cpp` (IPv4 fix), `BPTexture.cpp`, `Platform.cpp` (string / JSON helpers that replace NSString /
NSDictionary).

There is no Gradle project: the Java side is compiled with `javac` against the SDK's `android.jar` and turned
into a dex with `d8`. The game already contains ZXing (used for the QR codes), so ZXing is only needed to
compile and is not bundled.

## Building (GitHub Actions)

*Actions -> Build BowlingPlus (Android)*. It runs on every push that touches `android/`, `src/` or the
workflow, and by hand with *Run workflow*.

The game is **not** in the repo. Give the workflow a direct download link to the game's `.apks` / `.xapk`
bundle (or a single universal `.apk`):

- the repository secret `GAME_APK_URL` (*Settings -> Secrets and variables -> Actions*), **or**
- the `apk_url` box when you run it by hand.

The link has to download the file itself (not a web page). Without a link the workflow still builds and
uploads `libmain.so` and `bowlingplus.dex`, which shows whether everything compiles.

Output: the artifact **BowlingPlus-android** with `BowlingPlus.apk`.

### Keeping one signing key (recommended)

Each build is signed. Without your own key it uses a new throwaway key every time, and Android won't install
an APK over one signed with a different key: you'd have to uninstall first, which **erases the game's data**
on the phone. Make one key once:

```sh
keytool -genkeypair -keystore bowlingplus.keystore -alias bowlingplus -keyalg RSA -keysize 2048 -validity 10000 -dname CN=BowlingPlus
base64 -w0 bowlingplus.keystore      # macOS: base64 -i bowlingplus.keystore
```

and add these repository secrets: `ANDROID_KEYSTORE_B64` (the base64 text), `ANDROID_KEYSTORE_PASSWORD`,
`ANDROID_KEY_ALIAS` (`bowlingplus`), `ANDROID_KEY_PASSWORD` (the same password unless you chose another).
Keep the keystore file somewhere safe and never commit it.

## Building locally

Needs a JDK 17+, python3, cmake, curl and the Android SDK (`platforms;android-35`, `build-tools;35.0.0`,
`ndk;27.2.12479018`) with `ANDROID_HOME` set.

```sh
android/build.sh                          # bowlingplus.dex + libmain.so in android/out/
android/build.sh path/to/game.apks        # ...plus android/out/BowlingPlus.apk
BP_KEYSTORE=bowlingplus.keystore BP_KS_PASS=... android/build.sh game.apks   # with your key
```

`python3 android/tools/axml.py dump AndroidManifest.xml` prints any compiled Android XML as text.

## Installing

A re‑signed APK **cannot** install over the Play Store copy. Uninstall the store version first, then install
`BowlingPlus.apk`. Because the signature differs from the developer's, Google / Facebook sign‑in may not work
(this is the same situation as a sideloaded iOS build).

## What's different from iOS

Same features, a few platform‑dependent changes:

- **Privacy page**: the publisher's SDK shows the agreement in a `WebView`, loading the same HTML as iOS
  (`clickButton()` / "By clicking Sign up"), so the remember‑and‑auto‑press logic carries over
  (`Privacy.java`).
- **Skin files** load from inside the APK via Unity's `streamingAssetsPath` rather than the iOS bundle path.
- **120 FPS** asks the window for the display's fastest mode; there's no iOS‑style Info.plist flag, so the only
  requirement is a 120 Hz screen.
- **QR scanning**: instead of a live camera preview, you import a QR from a photo / screenshot or paste the
  text code. Encoding and decoding use ZXing.
- **Kegel PDF import**: uses Android 15's PDF text extraction where available, with a best‑effort fallback on
  older versions (the `.Pattern`, `.txt` and `.zip` imports work everywhere).

## Caveats

- arm64 only (the game ships arm64 libraries only).
- Built against the 1.907 build (Unity 6000.0.67f1); small game updates usually keep working because
  everything is looked up by name, not by address.
- If BowlingPlus ever fails to start the game twice in a row, it pauses itself ("safe mode") so the game keeps
  working; turn it back on from the shake menu.
