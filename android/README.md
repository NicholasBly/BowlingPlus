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

The game's `lib/arm64-v8a/libmain.so` is the Unity bootstrap that Java loads first (it calls `main`). The
patched APK renames the game's library to `libmain_orig.so` and ships **our** `libmain.so` in its place. Our
`JNI_OnLoad` (`android/native/Jni.cpp`):

1. `dlopen`s `libmain_orig.so` and forwards `JNI_OnLoad`, so Unity starts exactly as before;
2. starts the IPv4 DNS filter (it rebinds `getaddrinfo` in `libil2cpp.so`, the same data‑only trick as
   fishhook on iOS);
3. loads the BowlingPlus Java side — the shake menu and the oil / pin UI — which is compiled to a `classes.dex`
   and **embedded inside `libmain.so`**, so the whole tweak is one library and no `classes.dex` is edited.

The per‑frame work runs on Unity's own thread: Java posts a job every screen refresh via
`UnityPlayer.invokeOnMainThread(Runnable)`, which Unity drains between frames (the Android equivalent of the
iOS `CADisplayLink` on the main thread). Menu actions that touch the game are queued onto that same thread.

No Java class, resource or other library of the game is modified. The only manifest changes are a transparent
helper `<activity>` (for the photo / file pickers) and a small read‑only `<provider>` (for sharing QR codes
and the pin template), injected by `tools/manifest_patch.py`.

## Layout

```
android/
  native/     the tweak in C++ (shares src/ with the iOS build) -> libmain.so
  java/       the shake menu and oil / pin UI -> classes.dex (embedded in libmain.so)
  tools/      bin2header.py, manifest_patch.py, patch_apk.py
.github/workflows/build-android.yml
```

Native files: `Jni.cpp` (entry point, thread bridge), `Game.cpp` (the ported game logic), `Il2Cpp.cpp`
(by‑name IL2CPP lookups), `Engine.cpp` (settings + crash guard), `Log.cpp` (event log + connection test),
`Dns.cpp` (IPv4 fix), `BPTexture.cpp`, `Platform.cpp` (string / JSON helpers that replace NSString /
NSDictionary).

## Building (GitHub Actions)

The game APK is **not** committed. Provide it one of two ways:

- set a repository secret `GAME_APK_URL` to a direct link to the game's `.apks` / `.xapk` bundle (or a single
  universal `.apk`), **or**
- run the workflow manually (*Actions → Build BowlingPlus (Android) → Run workflow*) and paste a URL into
  `apk_url`.

The workflow compiles the Java to a dex, embeds it, builds the native library with the NDK, then merges the
split APKs (APKEditor), swaps in `libmain.so`, patches the manifest, aligns and signs, and uploads
`BowlingPlus.apk` as an artifact.

## Building locally

```sh
# 1) Java -> dex (needs the Android SDK + a Gradle install)
cd android/java && gradle assembleRelease
unzip -o build/outputs/apk/release/*.apk classes.dex -d .

# 2) embed the dex, then build the native library with the NDK
python3 ../tools/bin2header.py classes.dex kBPDex ../native/bp_dex.h
cd ../native
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 -DGENERATED_DIR=$PWD -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 3) merge + patch + sign (APKEditor.jar, Android build-tools on PATH)
python3 ../tools/patch_apk.py path/to/game.apks \
  --lib build/libmain.so --out BowlingPlus.apk --apkeditor path/to/APKEditor.jar
```

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
- Tested by construction against the 1.907 build's IL2CPP metadata; small game updates usually keep working
  because everything is looked up by name, not by address.
- If BowlingPlus ever fails to start the game twice in a row, it pauses itself ("safe mode") so the game keeps
  working; turn it back on from the shake menu.
