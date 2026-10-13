#!/usr/bin/env bash
# Builds NVIDIA PhysX 4.1 (the engine Unity 6 embeds) from GitHub as static libraries, then pinlab.
# Needs: git, cmake, clang (apt: clang-18 cmake). No NVIDIA servers needed (their packman step is skipped).
#   tools/dev/pinlab/build_physx.sh            (from the repo root; about 3 minutes on one core)
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="${PINLAB_WORK:-$HOME/.cache/pinlab}"
mkdir -p "$WORK"
if [ ! -d "$WORK/physx/physx/source" ]; then
    rm -rf "$WORK/physx"
    git clone -q --depth 1 --branch 4.1 --filter=blob:none --sparse https://github.com/NVIDIAGameWorks/PhysX.git "$WORK/physx"
    (cd "$WORK/physx" && git sparse-checkout set --no-cone '/physx/source/' '/physx/include/' '/physx/compiler/' '/physx/buildtools/' '/pxshared/' '/externals/cmakemodules/')
fi
R="$WORK/physx/physx"
# PhysX's CMake reads these from the environment (the -D forms below are not enough)
export PHYSX_ROOT_DIR="$R" PM_CMakeModules_PATH="$WORK/physx/externals/cmakemodules" PM_PxShared_PATH="$WORK/physx/pxshared"
if [ ! -f "$R/bin/linux.clang/release/libPhysX_static_64.a" ]; then
    mkdir -p "$R/build" && cd "$R/build"
    CC=${CC:-clang-18}; CXX=${CXX:-clang++-18}
    cmake ../compiler/public -G "Unix Makefiles" --no-warn-unused-cli -DPHYSX_ROOT_DIR="$R" -DPX_OUTPUT_LIB_DIR="$R" -DPX_OUTPUT_BIN_DIR="$R" \
        -DTARGET_BUILD_PLATFORM=linux -DPX_OUTPUT_ARCH=x86 -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_BUILD_TYPE=release \
        -DPX_BUILDSNIPPETS=False -DPX_BUILDPUBLICSAMPLES=False -DPX_GENERATE_STATIC_LIBRARIES=True -DNV_USE_STATIC_WINCRT=True \
        -DNV_USE_DEBUG_WINCRT=False -DPX_FLOAT_POINT_PRECISE_MATH=False -DPM_PxShared_PATH="$WORK/physx/pxshared" \
        -DPM_CMakeModules_PATH="$WORK/physx/externals/cmakemodules" -DCMAKE_PREFIX_PATH="$WORK/physx/externals/cmakemodules" -DCMAKE_CXX_FLAGS="-w" >/dev/null
    make -j"$(nproc)" PhysX PhysXCommon PhysXFoundation PhysXCooking PhysXExtensions PhysXPvdSDK >/dev/null
fi
L="$R/bin/linux.clang/release"
${CXX:-clang++-18} -O2 -std=c++17 -w -DNDEBUG -DPX_PHYSX_STATIC_LIB -I"$R/include" -I"$WORK/physx/pxshared/include" "$HERE/pinlab.cpp" -o "$HERE/pinlab" \
    -L"$L" -Wl,--start-group -lPhysXExtensions_static_64 -lPhysX_static_64 -lPhysXPvdSDK_static_64 -lPhysXCooking_static_64 \
    -lPhysXCommon_static_64 -lPhysXFoundation_static_64 -Wl,--end-group -lpthread -ldl
echo "built $HERE/pinlab"
