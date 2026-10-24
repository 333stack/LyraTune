#!/usr/bin/env bash
# JUCE uses `#pragma comment(lib, "D2d1.lib")` etc. The xwin SDK libs are lowercase and Linux
# filesystems are case-sensitive, so add case-matching symlinks for every lib JUCE names.
set -euo pipefail
XWIN_DIR="${XWIN_DIR:-$HOME/.local/share/xwin}"
JUCE_DIR="$(dirname "$0")/../third_party/JUCE/modules"
grep -rhoE 'pragma comment *\(lib, *"[^"]+"' "$JUCE_DIR" | grep -oE '"[^"]+"' | tr -d '"' | sort -u |
while read -r name; do
    [[ $name == *.lib ]] || name="$name.lib"
    for dir in "$XWIN_DIR"/sdk/lib/um/x86_64 "$XWIN_DIR"/crt/lib/x86_64; do
        lower="${name,,}"
        if [[ ! -e $dir/$name && -e $dir/$lower ]]; then ln -s "$lower" "$dir/$name"; echo "alias $dir/$name"; fi
    done
done
