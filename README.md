
# LyraTune

**A real-time automatic vocal tuner focused on low latency and natural sound.**

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
