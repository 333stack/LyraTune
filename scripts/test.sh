#!/usr/bin/env bash
# Native DSP unit tests. Usage: scripts/test.sh [--asan]
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ${1:-} == --asan ]]; then B=build/tests-asan; X=(-DCMAKE_BUILD_TYPE=Debug -DLYRA_SANITIZE=ON); else B=build/tests; X=(-DCMAKE_BUILD_TYPE=Release); fi
[[ -f $B/build.ninja ]] || cmake -S Tests -B "$B" -G Ninja "${X[@]}" >/dev/null
ninja -C "$B" >/dev/null
"$B/dsp_tests"
