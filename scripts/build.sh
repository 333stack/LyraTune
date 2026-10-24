#!/usr/bin/env bash
# Usage: scripts/build.sh [Release|Debug]   -> build/win-<config>/LyraTune_artefacts/<Config>/VST3/
set -euo pipefail
cd "$(dirname "$0")/.."
CONFIG="${1:-Release}"
BUILD="build/win-${CONFIG,,}"
[[ -f $BUILD/build.ninja ]] || cmake -S . -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/windows-x64-clang-cl.cmake -DCMAKE_BUILD_TYPE="$CONFIG"
# -j4: this machine has ~7 GB RAM; higher parallelism swaps heavily.
cmake --build "$BUILD" -j"${JOBS:-4}"
echo "Built: $(find "$BUILD/LyraTune_artefacts" -maxdepth 3 -name '*.vst3' -type d)"
