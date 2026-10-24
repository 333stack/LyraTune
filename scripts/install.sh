#!/usr/bin/env bash
# Usage: scripts/install.sh [Release|Debug]
# Linux + Wine: copies the built bundle into <WINEPREFIX>/drive_c/Program Files/Common Files/VST3.
# Set WINEPREFIX to the prefix your DAW runs in (default: ~/.wine). On Windows, copy the
# "Lyra Tune.vst3" folder to C:\Program Files\Common Files\VST3 instead.
set -euo pipefail
cd "$(dirname "$0")/.."
CONFIG="${1:-Release}"
PREFIX="${WINEPREFIX:-$HOME/.wine}"
DEST="$PREFIX/drive_c/Program Files/Common Files/VST3"
SRC="$(find "build/win-${CONFIG,,}/LyraTune_artefacts/$CONFIG/VST3" -maxdepth 1 -name '*.vst3' -type d | head -1)"
[[ -n $SRC ]] || { echo "No built bundle for $CONFIG; run scripts/build.sh $CONFIG" >&2; exit 1; }
[[ -d $PREFIX/drive_c ]] || { echo "Not a Wine prefix: $PREFIX (set WINEPREFIX)" >&2; exit 1; }
NAME="$(basename "$SRC")"

# Never replace the DLL while a process has it mapped (the host would crash or keep the old one).
if grep -lsF "$DEST/$NAME" /proc/[0-9]*/maps >/dev/null 2>&1; then
    echo "\"$NAME\" is loaded by a running process. Remove it from all tracks or close the DAW, then retry." >&2
    exit 1
fi

# Smoke-test the new build in the same prefix before replacing an existing install (optional tool).
if [[ -x build/vst3_load_smoke.exe ]]; then
    WINEPREFIX="$PREFIX" WINEDEBUG=-all timeout 60 wine build/vst3_load_smoke.exe \
        "Z:$PWD/$SRC/Contents/x86_64-win/$NAME" | grep -q '^OK' || { echo "Smoke test failed; not installing." >&2; exit 1; }
fi

mkdir -p "$DEST"
rm -rf "$DEST/$NAME.new" && cp -r "$SRC" "$DEST/$NAME.new"
rm -rf "$DEST/$NAME" && mv "$DEST/$NAME.new" "$DEST/$NAME"
echo "Installed: $DEST/$NAME"
echo "Rescan plugins in your DAW (or re-insert the plugin after an update)."
