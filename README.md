
# LyraTune

**A real-time automatic vocal tuner focused on low latency and natural sound.**

![Lyra Tune interface](docs/screenshot.png)

LyraTune is a real-time pitch-correction plugin designed to deliver responsive vocal tuning, from transparent correction to aggressive hard-tuned effects.

## Features

### 🎯 Low-Latency Pitch Correction
- **Honest latency reporting:** 3.6 ms / 5.3 ms / 7.4 ms of added latency at 48 kHz across High, Mid, and Low quality ranges, respectively.
- **Host delay compensation:** Reports plugin latency to the host for proper plugin delay compensation (PDC).
- **Adjustable tuning behavior:** Fine-tune correction speed and amount for everything from natural vocals to robotic effects.
- **Aggressive tuning:** Supports correction speeds down to -3 ms for an especially immediate, hard-tuned sound.

### 🎵 Accurate Pitch Detection
- **YIN-based pitch detection** for real-time vocal pitch tracking.
- **Octave-error protection** to help prevent incorrect octave detection.
- Designed for responsive tracking during live vocal monitoring.

### 🎹 Flexible Scale Control
- Chromatic, major, minor, and fully custom scales.
- Click individual keys to allow or forbid notes.
- Right-click a key to set the scale root.
- Customize pitch correction to fit your song and vocal performance.

### 📊 Real-Time Visual Feedback
- **Animated pitch orb** for live pitch visualization.
- **Input and output HeatMaps** driven by real DSP data.
- **Detected-note display** for immediate feedback on pitch tracking and correction.

### ⚡ Real-Time Audio Safety
- No memory allocations on the audio thread.
- No locks on the audio thread.
- Automated tests to verify real-time safety requirements.

### 🛠️ Workflow and Usability
- Undo and redo support.
- A/B snapshots for comparing settings.
- Reset all controls to their default values.
- Resizable interface with scaling from 75% to 150%.
- Tooltips for controls and parameters.

## Technical Overview

| Component | Implementation |
|---|---|
| Pitch detection | YIN-based |
| Processing | Real-time pitch correction |
| Latency at 48 kHz | 3.6 / 5.3 / 7.4 ms |
| Latency compensation | Host-reported plugin latency |
| Scale modes | Chromatic, major, minor, custom |
| Audio-thread allocations | None |
| Audio-thread locks | None |
| Verification | Automated tests |

## Credits

- **Lead design and engineering:** stack
- **Development assistance:** Claude (AI assistant)

## Download & Install

Get `LyraTune-0.1.0-win-x64-vst3.zip` from the [Releases](https://github.com/333stack/LyraTune/releases) page.

- **Windows:** unzip and copy the `Lyra Tune.vst3` folder to `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW.
- **Linux (Windows DAW under Wine):** copy it to the same folder inside the DAW's Wine prefix:
  `<prefix>/drive_c/Program Files/Common Files/VST3/`.

Format: VST3, Windows x64. Verify downloads with `SHA256SUMS.txt`.

## Build from Source

Cross-compiled on Linux with clang-cl + lld and `xwin`:

```sh
git clone --recursive https://github.com/333stack/LyraTune.git && cd LyraTune
xwin --accept-license --arch x86_64 --cache-dir ~/.cache/xwin splat --output ~/.local/share/xwin
scripts/setup-xwin-aliases.sh
scripts/build.sh      # -> build/win-release/LyraTune_artefacts/Release/VST3/Lyra Tune.vst3
scripts/test.sh       # native unit tests
```

## Documentation

Full user guide, DSP design, latency tables, measurements, known limitations and roadmap:
**[LyraTune.md](LyraTune.md)**.

## License

GNU Affero General Public License v3 — see [LICENSE](LICENSE).
Third-party components and notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
