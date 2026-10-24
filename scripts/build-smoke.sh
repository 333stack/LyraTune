#!/usr/bin/env bash
# Builds build/vst3_load_smoke.exe: loads a .vst3 DLL under Wine and lists its classes.
set -euo pipefail
cd "$(dirname "$0")/.."
X="${XWIN_DIR:-$HOME/.local/share/xwin}"
mkdir -p build
clang-cl --target=x86_64-pc-windows-msvc -fuse-ld=lld-link /MT /O2 \
    /imsvc"$X/crt/include" /imsvc"$X/sdk/include/ucrt" /imsvc"$X/sdk/include/um" /imsvc"$X/sdk/include/shared" \
    Tests/smoke/vst3_load_smoke.cpp /Fobuild/ /Fe:build/vst3_load_smoke.exe \
    /link /libpath:"$X/crt/lib/x86_64" /libpath:"$X/sdk/lib/um/x86_64" /libpath:"$X/sdk/lib/ucrt/x86_64"
echo "Built: build/vst3_load_smoke.exe"
