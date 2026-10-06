#!/bin/sh
# Host compile + link of the Android native code with stub JNI/Android headers (no NDK needed). Catches
# syntax/type errors and missing symbols; the only exported symbol must be JNI_OnLoad.
# Usage (from the repo root):  sh tools/dev/host_check.sh
set -e
STUBS="$(pwd)/tools/dev/stubs"
cd android/native
for f in *.cpp; do g++ -std=c++17 -fsyntax-only -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
    -Wno-missing-field-initializers -Wno-sign-compare -I"$STUBS" -I. -I../../src "$f"; done
OBJ=$(mktemp -d)
for f in *.cpp; do g++ -std=c++17 -O1 -fPIC -fvisibility=hidden -w -I"$STUBS" -I. -I../../src -c "$f" -o "$OBJ/${f%.cpp}.o"; done
g++ -shared -o "$OBJ/libmain.so" "$OBJ"/*.o -lpthread -ldl
echo "exports:"; nm -D -C --defined-only "$OBJ/libmain.so" | grep " T "
