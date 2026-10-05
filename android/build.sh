#!/usr/bin/env bash
# Builds BowlingPlus for Android. The same script runs on GitHub Actions and on your own machine.
#
#   android/build.sh                      # build the Java side (dex) and the native library only
#   android/build.sh path/to/game.apks    # ...then patch the game: android/out/BowlingPlus.apk
#
# Needs: a JDK (17+), python3, cmake, curl, and the Android SDK with
#   platforms;android-35  build-tools;35.0.0  ndk;27.2.12479018
# (ANDROID_HOME or ANDROID_SDK_ROOT pointing at it). Override with BP_PLATFORM / BP_BUILD_TOOLS / ANDROID_NDK.
# Signing: see android/README.md (BP_KEYSTORE, BP_KS_PASS, BP_KEY_ALIAS, BP_KEY_PASS); default is a throwaway key.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
[ -n "$SDK" ] && [ -d "$SDK" ] || { echo "error: set ANDROID_HOME to your Android SDK" >&2; exit 1; }
PLATFORM="${BP_PLATFORM:-android-35}"
BUILD_TOOLS="$SDK/build-tools/${BP_BUILD_TOOLS:-35.0.0}"
NDK="${ANDROID_NDK:-$SDK/ndk/27.2.12479018}"
AJAR="$SDK/platforms/$PLATFORM/android.jar"
OUT="${BP_OUT:-$HERE/out}"
ZXING_VERSION=3.5.3   # compile-time only: the game already ships ZXing, so it isn't bundled

for f in "$AJAR" "$BUILD_TOOLS/d8" "$NDK/build/cmake/android.toolchain.cmake"; do
    [ -e "$f" ] || { echo "error: missing $f (install it with sdkmanager)" >&2; exit 1; }
done
mkdir -p "$OUT"

echo "== 1/3 Java side -> bowlingplus.dex"
ZXING="$OUT/cache/zxing-core-$ZXING_VERSION.jar"
if [ ! -s "$ZXING" ]; then
    mkdir -p "$OUT/cache"
    curl -fsSL --retry 3 -o "$ZXING" "https://repo1.maven.org/maven2/com/google/zxing/core/$ZXING_VERSION/core-$ZXING_VERSION.jar"
fi
rm -rf "$OUT/classes" "$OUT/dex" && mkdir -p "$OUT/classes" "$OUT/dex"
mapfile -t SOURCES < <(find "$HERE/java" -name '*.java')
javac --release 11 -encoding UTF-8 -Xlint:-options -cp "$AJAR:$ZXING" -d "$OUT/classes" "${SOURCES[@]}"
mapfile -t CLASSES < <(find "$OUT/classes" -name '*.class')
"$BUILD_TOOLS/d8" --release --min-api 23 --lib "$AJAR" --classpath "$ZXING" --output "$OUT/dex" "${CLASSES[@]}"
[ -f "$OUT/dex/classes.dex" ] && [ ! -f "$OUT/dex/classes2.dex" ] || { echo "error: expected exactly one classes.dex from d8" >&2; exit 1; }
cp "$OUT/dex/classes.dex" "$OUT/bowlingplus.dex"
ls -la "$OUT/bowlingplus.dex"

echo "== 2/3 native library -> libmain.so (arm64-v8a)"
cmake -S "$HERE/native" -B "$OUT/native" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 -DCMAKE_BUILD_TYPE=Release
cmake --build "$OUT/native" -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cp "$OUT/native/libmain.so" "$OUT/libmain.so"
STRIP="$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip 2>/dev/null | head -n1 || true)"
[ -n "$STRIP" ] && "$STRIP" --strip-unneeded "$OUT/libmain.so"
ls -la "$OUT/libmain.so"

if [ "$#" -eq 0 ]; then
    echo "== 3/3 skipped: no game APK given (android/build.sh path/to/game.apks to make BowlingPlus.apk)"
    exit 0
fi
echo "== 3/3 patching the game -> BowlingPlus.apk"
python3 "$HERE/tools/patch_apk.py" "$@" \
    --lib "$OUT/libmain.so" --dex "$OUT/bowlingplus.dex" --out "$OUT/BowlingPlus.apk" \
    --apksigner "$BUILD_TOOLS/apksigner" --zipalign "$BUILD_TOOLS/zipalign"
ls -la "$OUT/BowlingPlus.apk"
