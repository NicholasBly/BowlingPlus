cmake_minimum_required(VERSION 3.22)
project(bowlingplus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The output must be called libmain.so: Unity's Java loads "main". tools/patch_apk.py renames the game's own
# libmain.so to libmain_orig.so, which this library dlopen()s and forwards JNI_OnLoad to.
add_library(main SHARED
    Jni.cpp
    Engine.cpp
    Game.cpp
    Il2Cpp.cpp
    Log.cpp
    Dns.cpp
    BPTexture.cpp
    Platform.cpp
)

# ../../src holds the plain C/C++ headers shared with the iOS build (KegelParse.h, PinWrap.h, PinMesh.h,
# PinMask.h, PinGuide.h, Logo.h).
target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

target_compile_options(main PRIVATE
    -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
    -Wno-missing-field-initializers -Wno-sign-compare
    -fvisibility=hidden -ffunction-sections -fdata-sections
)

# Keep only JNI_OnLoad exported. 16 KB page alignment so it also loads on 16 KB-page devices (Android 15+).
target_link_options(main PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384)

target_link_libraries(main log android)
