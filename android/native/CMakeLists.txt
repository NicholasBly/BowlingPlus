cmake_minimum_required(VERSION 3.22)
project(bowlingplus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The output must be called libmain.so: the game's Java loads "main". The patch script renames the game's
# own libmain.so to libmain_orig.so, which this library dlopen()s and forwards JNI_OnLoad to.
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

# src/ holds the plain-C headers shared with the iOS build (KegelParse.h, PinWrap.h, PinMesh.h, PinMask.h,
# PinGuide.h, Logo.h); GENERATED is where bp_dex.h lands (the embedded Java side).
target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${GENERATED_DIR}
)

target_compile_options(main PRIVATE
    -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
    -Wno-missing-field-initializers -Wno-sign-compare
    -fvisibility=hidden -ffunction-sections -fdata-sections
    $<$<CONFIG:Release>:-O2>
)

# Keep JNI_OnLoad exported; drop everything else.
target_link_options(main PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)

target_link_libraries(main log android)
