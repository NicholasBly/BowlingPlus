#!/usr/bin/env bash
# Builds BowlingPlus for Android where Google's and Maven's servers are blocked and only GitHub (incl. release
# assets and codeload), PyPI, npm, crates and the Ubuntu archive can be reached (e.g. a Claude chat's sandbox).
# Same outputs as android/build.sh: libmain.so, bowlingplus.dex and, given the game, BowlingPlus.apk.
#
#   tools/dev/android_offline_build.sh                      # libmain.so + bowlingplus.dex only
#   tools/dev/android_offline_build.sh path/to/game.apks    # ...and the patched, signed BowlingPlus.apk
#
# Ubuntu 24.04 (x86-64). Where each piece comes from:
#   JDK 17, clang-18, lld-18, llvm-18          Ubuntu archive (apt)
#   d8.jar, apksigner.jar, zipalign (static)   AndroidIDE build-tools 34.0.4 x86_64 (GitHub release)
#   android.jar (API 35)                       Reginer/aosp-android-jar (raw.githubusercontent.com)
#   ZXing 3.5.3 (compile-time only)            built from its GitHub source (codeload.github.com)
#   NDK sysroot + compiler-rt + libunwind      termux-ndk r29 (GitHub release). Only the target files are used:
#                                              headers, bionic stubs, libc++ and the runtime archives, which don't
#                                              depend on the host; the compiler is Ubuntu's clang-18.
# Signing: BP_KEYSTORE / BP_KS_PASS / BP_KEY_ALIAS / BP_KEY_PASS as in android/README.md; default is a throwaway key.
# Cache: $BP_CACHE (default ~/.cache/bowlingplus-offline), about 750 MB.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE="${BP_CACHE:-$HOME/.cache/bowlingplus-offline}"
OUT="${BP_OUT:-$ROOT/android/out}"
mkdir -p "$CACHE" "$OUT"

echo "== tools"
need=""
for t in javac clang++-18 ld.lld-18 llvm-strip-18 curl python3; do command -v "$t" >/dev/null || need=1; done
if [ -n "$need" ]; then
    SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO=sudo
    $SUDO apt-get update -qq || echo "note: apt-get update had errors (a blocked third-party list in /etc/apt/sources.list.d? move it aside)"
    $SUDO apt-get install -y -qq openjdk-17-jdk-headless clang-18 lld-18 llvm-18 curl python3 >/dev/null
fi

BT="$CACHE/build-tools/34.0.4"
if [ ! -f "$BT/lib/d8.jar" ]; then
    curl -fsSL https://github.com/AndroidIDEOfficial/androidide-tools/releases/download/v34.0.4/build-tools-34.0.4-x86_64.tar.xz | tar -xJf - -C "$CACHE"
fi
AJAR="$CACHE/android-35.jar"
[ -s "$AJAR" ] || curl -fsSL -o "$AJAR" https://raw.githubusercontent.com/Reginer/aosp-android-jar/main/android-35/android.jar
ZXING="$CACHE/zxing-core-3.5.3.jar"
if [ ! -s "$ZXING" ]; then
    rm -rf "$CACHE/zxing-src" "$CACHE/zxing-classes" && mkdir -p "$CACHE/zxing-src" "$CACHE/zxing-classes"
    curl -fsSL https://codeload.github.com/zxing/zxing/tar.gz/refs/tags/zxing-3.5.3 | tar -xzf - -C "$CACHE/zxing-src"
    find "$CACHE/zxing-src" -path '*/core/src/main/java/*.java' > "$CACHE/zxing-src/list.txt"
    javac --release 11 -encoding UTF-8 -nowarn -d "$CACHE/zxing-classes" @"$CACHE/zxing-src/list.txt"
    (cd "$CACHE/zxing-classes" && jar cf "$ZXING" .)
fi
if ! ls -d "$CACHE"/ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr/include >/dev/null 2>&1; then
    mkdir -p "$CACHE/ndk"
    curl -fsSL https://github.com/lzhiyong/termux-ndk/releases/download/android-ndk/android-ndk-r29-aarch64.tar.xz |
        tar -xJf - -C "$CACHE/ndk" --wildcards '*/toolchains/llvm/prebuilt/*/sysroot/usr/include/*' \
            '*/toolchains/llvm/prebuilt/*/sysroot/usr/lib/aarch64-linux-android/*' \
            '*/toolchains/llvm/prebuilt/*/lib/clang/*/lib/linux/*aarch64*'
    # (the archive's prebuilt folder is named linux-x86_64 even though its host binaries are aarch64)
fi
NDKP="$(dirname "$(dirname "$(ls -d "$CACHE"/ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr | head -n1)")")"
RES="$(ls -d "$NDKP"/lib/clang/* | head -n1)"
SYS="$NDKP/sysroot"

echo "== 1/3 Java side -> bowlingplus.dex"
rm -rf "$OUT/classes" "$OUT/dex" && mkdir -p "$OUT/classes" "$OUT/dex"
find "$ROOT/android/java" -name '*.java' > "$OUT/sources.txt"
javac --release 11 -encoding UTF-8 -Xlint:-options -cp "$AJAR:$ZXING" -d "$OUT/classes" @"$OUT/sources.txt"
find "$OUT/classes" -name '*.class' > "$OUT/classes.txt"
java -cp "$BT/lib/d8.jar" com.android.tools.r8.D8 --release --min-api 23 --lib "$AJAR" --classpath "$ZXING" --output "$OUT/dex" @"$OUT/classes.txt"
[ -f "$OUT/dex/classes.dex" ] && [ ! -f "$OUT/dex/classes2.dex" ] || { echo "error: expected exactly one classes.dex" >&2; exit 1; }
cp "$OUT/dex/classes.dex" "$OUT/bowlingplus.dex"
ls -la "$OUT/bowlingplus.dex"

echo "== 2/3 native library -> libmain.so (arm64-v8a)"
OBJ="$OUT/native-obj" && rm -rf "$OBJ" && mkdir -p "$OBJ"
CF=(--target=aarch64-linux-android23 "--sysroot=$SYS" -std=c++17 -O3 -DNDEBUG -fPIC -fvisibility=hidden -ffunction-sections
    -fdata-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -Wformat
    -Werror=format-security -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
    -Wno-missing-field-initializers -Wno-sign-compare -I"$ROOT/android/native" -I"$ROOT/src")
for f in "$ROOT"/android/native/*.cpp; do clang++-18 "${CF[@]}" -c "$f" -o "$OBJ/$(basename "${f%.cpp}").o"; done
clang++-18 --target=aarch64-linux-android23 "--sysroot=$SYS" -shared -fuse-ld=lld -rtlib=compiler-rt --unwindlib=libunwind \
    -resource-dir="$RES" -L"$RES/lib/linux/aarch64" -static-libstdc++ -Wl,--gc-sections -Wl,--exclude-libs,ALL \
    -Wl,-z,max-page-size=16384 -Wl,--build-id=sha1 -Wl,--no-undefined -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now \
    -Wl,-soname,libmain.so -o "$OUT/libmain.so" "$OBJ"/*.o -llog -landroid
llvm-strip-18 --strip-unneeded "$OUT/libmain.so"
EXPORTS="$(llvm-nm-18 -D --defined-only "$OUT/libmain.so" | awk '$2 == "T" { print $3 }')"
[ "$EXPORTS" = "JNI_OnLoad" ] || { echo "error: libmain.so must export only JNI_OnLoad, got: $EXPORTS" >&2; exit 1; }
ls -la "$OUT/libmain.so"

if [ "$#" -eq 0 ]; then
    echo "== 3/3 skipped: no game APK given"
    exit 0
fi
echo "== 3/3 patching the game -> BowlingPlus.apk"
printf '#!/bin/sh\nexec java -jar "%s" "$@"\n' "$BT/lib/apksigner.jar" > "$CACHE/apksigner" && chmod +x "$CACHE/apksigner"
# This zipalign predates -P 16, so patch_apk.py falls back to -p (4 KB). That is fine for this game: its manifest
# has extractNativeLibs="true", so Android extracts the libraries at install instead of mapping them from the APK.
python3 "$ROOT/android/tools/patch_apk.py" "$@" --lib "$OUT/libmain.so" --dex "$OUT/bowlingplus.dex" \
    --out "$OUT/BowlingPlus.apk" --apksigner "$CACHE/apksigner" --zipalign "$BT/zipalign" 2>&1 | grep -v "not protected by signature"
rm -f "$OUT/BowlingPlus.apk.idsig"
ls -la "$OUT/BowlingPlus.apk"
