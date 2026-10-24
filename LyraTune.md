# Lyra Tune — Documentation

**Version 0.2.0** · Real-time automatic vocal pitch correction · VST3 (Windows x64)

Lead design and engineering: **stack**
Development assistance: Claude (AI assistant)

---

## Contents

1. [Overview](#1-overview)
2. [Feature status](#2-feature-status)
3. [Using Lyra Tune](#3-using-lyra-tune)
4. [Parameters and automation](#4-parameters-and-automation)
5. [Latency](#5-latency)
6. [How it works (DSP)](#6-how-it-works-dsp)
7. [User interface architecture](#7-user-interface-architecture)
8. [Real-time safety](#8-real-time-safety)
9. [State, snapshots and undo](#9-state-snapshots-and-undo)
10. [Building](#10-building)
11. [Installing](#11-installing)
12. [Testing and measurements](#12-testing-and-measurements)
13. [Project structure](#13-project-structure)
14. [Known limitations](#14-known-limitations)
15. [Roadmap](#15-roadmap)
16. [Changelog](#16-changelog)
17. [Credits and licenses](#17-credits-and-licenses)

---

## 1. Overview

Lyra Tune is a low-latency automatic pitch corrector for monophonic vocals. It detects the sung
pitch, picks the nearest allowed note from the selected key and scale, and shifts the voice toward
it with a controllable speed — from transparent, natural correction to the hard, stepped
"tuned vocal" effect.

Design goals, in priority order:

1. Real-time safety and low latency (usable for live monitoring).
2. Correct, stable, natural-sounding correction.
3. Predictable CPU use at every sample rate and buffer size.
4. Musically meaningful controls.
5. A polished, dark, focused interface.

The plugin is built as a **Windows x64 VST3**. It is developed on Linux and cross-compiled; it runs
in Windows DAWs, including Windows DAWs running under Wine on Linux (the development and test setup).

---

## 2. Feature status

Status levels: **Implemented** (code exists) · **Unit-tested** · **Measured** (numbers in §12) ·
**Host-tested** (used in a real DAW session).

| Feature | Status |
|---|---|
| Real-time pitch detection (YIN), voicing, confidence | Unit-tested, measured, host-tested |
| Octave-error protection | Unit-tested, measured |
| Key / scale: Chromatic, Major, Minor, Custom (per-note) | Unit-tested, host-tested |
| Root note, reference pitch (420–460 Hz) | Unit-tested |
| Speed (−3 … 400 ms) incl. aggressive negative range, 0 ms detent | Unit-tested, host-tested |
| Note Stabilizer (None / Short / Mid / Long), zero added latency | Unit-tested, measured |
| Sustain (−100 … +100, note-age-dependent speed) | Unit-tested |
| Amount (0–100 %) | Unit-tested, host-tested |
| Range (Low / Mid / High) — sets latency | Unit-tested, measured |
| Low-latency pitch shifter, latency reported to host | Unit-tested, measured, host-tested |
| Click-free bypass, latency-aligned dry path | Unit-tested |
| Custom UI: orb, knobs, keyboard, HeatMaps, detected notes | Host-tested (visual) |
| Undo / Redo, Reset All, A/B snapshots | Implemented |
| GUI scaling 75–150 %, tooltips | Implemented |
| Versioned state save/restore | Implemented (project reload not yet verified in a host) |
| HQ mode (2 ms detector lookahead) | Unit-tested, measured |
| Doubler (two drifting, detuned voices; Width, Blend) | Unit-tested, measured, host-tested (visual) |
| Presets, groups, reference tone | Planned (§15) |

---

## 3. Using Lyra Tune

Insert Lyra Tune on a **dry, monophonic** vocal track (before reverb, delay or doubling effects).

### Quick start

1. Set **Root** and **Scale** to the song's key (or leave **Chromatic**).
2. Set **Range** to the lowest register the singer uses — **High** gives the lowest latency for most
   lead vocals (anything above ~150 Hz, roughly D3).
3. Turn **Speed** clockwise for a harder tune, counter-clockwise for natural correction.
4. Use **Amount** to blend how much of the correction is applied.
5. Optional: **Note Stabilizer** (Short/Mid/Long) keeps fast runs natural; **Sustain** tightens (+) or
   loosens (−) long held notes.

### Window layout

**Header**
- **Menu** (left): window size 75 / 100 / 125 / 150 %.
- **Undo / Redo / Reset All**: edit history (≥ 20 steps); Reset All is undoable.
- **A / B** with the chevron between them: switch between two complete settings snapshots; the
  chevron copies the current snapshot to the other one.
- **?**: show or hide tooltips.

**Orb (left)** — a live view of what the tuner is doing:
- Centre: the target note (e.g. `E4`) and how far the sung pitch is from it in cents (`+28 ct`).
- Pink arc on the ring: runs from the detected pitch (small grey dot) to the corrected pitch (white
  dot). Its length is the correction being applied.
- Core colours rotate with the output note; brightness follows the input level.
- The grey handle is an **XY pad**: drag right for faster Speed, up for more Amount. Double-click it
  to reset both.
- While the Note Stabilizer is confirming a new note, the centre shows it dimmed with "confirming".
- The ring shifts toward pink as Speed goes into the aggressive range.
- Shows **BYPASSED** when the plugin is bypassed.

**Speed knob (large)** — how fast pitch is pulled to the target.
- Clockwise = faster/harder. The arc fills as the setting becomes more aggressive.
- `400 … 0 ms`: smoothing time constant. Slow settings keep vibrato and natural movement.
- `0 ms`: instant correction.
- `0 … −3 ms`: aggressive mode — instant, and the note-change hysteresis shrinks to zero at −3 ms,
  so notes snap the moment the voice crosses the midpoint.
- A notch marks **0 ms**; drags snap to it within ±0.4 ms. Past it the knob "overloads": the
  negative zone runs red-hot and flickers, a glow pulses behind the dial, and a shockwave fires as
  you cross zero — stronger the deeper you go.

**Sustain knob (small, left)** — changes Speed as a note is held. `+` retunes long notes faster
(up to 8× after 0.5 s), `−` lets them drift more naturally (up to 8× slower). The arc fills from the
centre. No effect when Speed is 0 or below (already instant).

**Amount knob (small)** — 0 % leaves pitch untouched; 100 % applies the full correction.

Knob gestures: drag up/down or left/right · **Shift**-drag = fine · **double-click** = default ·
**right-click** = type a value.

**Mode row**
- **Scale**: Chromatic · Major · Minor · Custom (Custom uses the notes enabled on the keyboard).
- **Stabilizer**: None · Short (40 ms) · Mid (80 ms) · Long (200 ms) — a new note must last this
  long before it is tuned; shorter notes, runs and scoops pass naturally. Adds **no** latency.
- **Range**: Low (70 Hz) · Mid (100 Hz) · High (150 Hz) — the lowest pitch tracked. Notes below the
  floor are left uncorrected. The added latency is shown live at the bottom-left of the main panel.

**Keyboard area**
- **Input / Output HeatMaps**: recent detected pitch (top) and corrected pitch (bottom) across one
  octave. Off-key singing shows as a smear between notes on Input and tight bars on Output.
- **Note labels**: the detected input note gets a white underline.
- **Keys**: white/charcoal = allowed, grey/deep black = not allowed. The current target glows violet.
  The root has a violet dot.
- **Click a key** to allow or forbid that note (switches to Custom, starting from the current scale).
- **Right-click a key** to make it the root (a Custom scale is transposed along with it).
- If every note is disabled, correction pauses and the keyboard says so.

**Left column** — the big root letter and scale name. Click it to open the root menu; click it
again to close the menu.

**Right column** — HeatMaps toggle · Detected Notes toggle · **Global Bypass** · **Pitch Reference**
(A4 frequency; drag, Shift-drag for fine, double-click = 440 Hz).

### Tips
- Keep the input dry and monophonic; reverb and harmonies confuse any pitch detector.
- For a hard-tuned effect: Speed 0 to −3 ms, Amount 100 %, correct key. Add Stabilizer Short or
  Mid so fast runs don't flip between notes.
- For "natural start, locked finish": Speed 40–80 ms with Sustain +50 or more.
- For transparent correction: Speed 30–80 ms, Amount 70–100 %.
- Change **Range** while playback is stopped: it changes the reported latency.

---

## 4. Parameters and automation

Parameter IDs are stable and must never be renamed — saved projects and automation refer to them.

| ID | Name | Range | Default | Automatable |
|---|---|---|---|---|
| `bypass` | Bypass | off / on | off | yes (host bypass) |
| `speed` | Speed | −3 … 400 ms (skewed, centre 40 ms) | 20 ms | yes |
| `amount` | Amount | 0 … 100 % | 100 % | yes |
| `root` | Root | C … B | C | yes |
| `scale` | Scale | Chromatic, Major, Minor, Custom | Chromatic | yes |
| `note0` … `note11` | Note C … Note B | off / on | on | yes |
| `refHz` | Reference | 420 … 460 Hz | 440 Hz | yes |
| `stabilizer` | Note Stabilizer | None, Short (40 ms), Mid (80 ms), Long (200 ms) | None | yes |
| `sustain` | Sustain | −100 … +100 | 0 | yes |
| `range` | Range | Low (70 Hz), Mid (100 Hz), High (150 Hz) | Low | **no** — it changes latency |
| `hq` | HQ Mode | off / on (+2 ms latency) | off | **no** — it changes latency |
| `doubler` | Doubler | off / on | off | yes |
| `dblWidth` | Doubler Width | 0 … 100 % | 70 % | yes |
| `dblBlend` | Doubler Blend | 0 … 100 % | 50 % | yes |

Display preferences (HeatMaps, Detected Notes, tooltips, window size) are saved with the project but
are not parameters.

---

## 5. Latency

Lyra Tune adds a **fixed, reported** latency so the host's delay compensation keeps it in time.
It does not change during playback unless **Range** is changed. The dry/bypass path is delayed by the
same amount, so bypassing never shifts timing.

Added latency (samples / milliseconds):

| Sample rate | Low (70 Hz) | Mid (100 Hz) | High (150 Hz) |
|---|---|---|---|
| 44.1 kHz | 328 / 7.44 ms | 233 / 5.28 ms | 160 / 3.63 ms |
| 48 kHz | 357 / 7.44 ms | 254 / 5.29 ms | 174 / 3.62 ms |
| 88.2 kHz | 653 / 7.40 ms | 464 / 5.26 ms | 317 / 3.59 ms |
| 96 kHz | 711 / 7.41 ms | 505 / 5.26 ms | 345 / 3.59 ms |
| 192 kHz | 1420 / 7.40 ms | 1008 / 5.25 ms | 688 / 3.58 ms |

What these numbers are — and are not:
- **Algorithmic latency** of the pitch shifter, reported to the host. Verified by a unit test that
  the shifter's mean delay equals the reported value at every ratio and pitch.
- **Not** total monitoring latency: audio interface and host buffers add their own delay.
- **Response time** is separate: the detector needs one period of the lowest note plus up to one
  analysis hop (2 ms) to make a decision. Correction reaches the target according to Speed.
- A time-domain shifter's instantaneous delay wanders by up to ± half a pitch period around the
  reported value; the average is exact.

Why Range sets latency: the shifter needs about half a period of the lowest supported note in
reserve (`latency ≈ 2 + 0.24 ms·rate + ½ · rate / Range floor` samples). Raising the floor from
70 to 150 Hz halves the latency.

---

## 6. How it works (DSP)

All DSP lives in `Source/DSP/`, independent of the plugin framework, so the plugin, the offline
tools and the unit tests run identical code.

```
input (mono or stereo)
 ├─► mono sum ─► YinDetector ─► CorrectionEngine ─► pitch ratio per sample
 │                  │ period                              │
 │                  ▼                                     ▼
 ├──────────────► PitchShifter (stereo-linked, one read head) ─► wet
 └─► FixedDelay (same latency) ─────────────────────────────────► dry
                                         wet/dry bypass crossfade (10 ms) ─► output
```
Processing runs in 32-sample sub-blocks (control rate ≈ 0.67 ms at 48 kHz), independent of the host
buffer size.

### 6.1 Pitch detection — `YinDetector`
- Pre-filter: 40 Hz high-pass + 4th-order Butterworth low-pass (≤ 4.5 kHz), then decimation to
  22–24 kHz. CPU cost is therefore nearly the same at 44.1 and 192 kHz.
- YIN (cumulative mean normalised difference function), absolute threshold 0.15, parabolic
  interpolation for sub-sample period precision. Voiced if the dip is below 0.30.
- **End-aligned** difference window: every lag compares the newest samples, so each estimate
  describes the most recent audio. (A start-aligned window lagged by several milliseconds and was
  measurably worse on glides — see §12.)
- New estimate every **2 ms**.
- Silence gate at −55 dBFS.
- Octave protection: (1) prefer twice the period when the half-period dip is much shallower than the
  full one; (2) a 30 ms continuity hold keeps the previous octave when the estimate jumps by an
  octave but the old period is still valid. Holding the lower octave is safe for the shifter (a
  whole multiple of the period still splices in phase).
- Non-finite input (NaN/Inf) is replaced by silence before it reaches any filter state.

### 6.2 Target selection — `TargetSelector`, `Scale.h`
- Pitch → fractional MIDI: `m = 69 + 12·log2(f / ref)`; cents are never rounded away.
- Allowed notes are a 12-bit pitch-class mask (Major/Minor are rotations of a C mask; Custom is the
  twelve note parameters).
- Nearest allowed note wins; exact ties go to the lower note.
- Hysteresis: the target only changes once the pitch is 15 cents **past** the midpoint between two
  notes (shrinks to 0 in aggressive Speed). If the current target is removed from the scale it is
  abandoned immediately. Empty mask → no correction.

### 6.3 Correction — `CorrectionEngine`
- Requested correction = `(target − input) · Amount`, in semitones.
- The **correction offset** (not the output pitch) is smoothed with a one-pole filter,
  `α = 1 − exp(−1 / (T · fs))`, `T = Speed`. Smoothing the offset lets fast natural movement such as
  vibrato pass through at slow speeds.
- Speed ≤ 0 never reaches the exponential: 0 → instant; −3…0 → instant plus reduced hysteresis.
- Unvoiced or low-confidence (< 0.70) frames release the correction to zero with
  `max(Speed, 20 ms)`.

**Note Stabilizer** (causal, zero added latency). When the selected note differs from the confirmed
target it becomes a *candidate*. While unconfirmed, the correction releases toward zero, so the
natural pitch passes. After the candidate persists for the threshold (40 / 80 / 200 ms) it is
confirmed and tuned. A note starting after ≥ 60 ms of unvoiced input is confirmed immediately, so
only note-to-note transitions wait; short consonant gaps inside a phrase keep the stabilizer engaged.
A lookahead design would delay all audio by the threshold; this one never delays anything, so it stays
available at every Range setting.

**Sustain** tracks the age of the confirmed note and scales Speed's time constant:
`T_eff = T · 2^(−3 · sustain · min(1, age / 0.5 s))` with sustain in −1…1. At +100 a note held for
0.5 s retunes 8× faster; at −100, 8× slower. Parameter changes act through the one-pole smoother, so
they never click.

### 6.4 Pitch shifting — `PitchShifter`
- Variable-speed delay-line read head (4-point Hermite interpolation). The read head advances
  `ratio` samples per output sample, so the delay drifts.
- When the delay leaves `[D − P/2, D + P/2]` (D = reported latency, P = detected period) the head
  jumps by whole periods back toward D, with a 4 ms crossfade. Period-aligned jumps keep the
  waveform in phase, so splices are inaudible on voiced material. Large upward ratios shorten the
  crossfade to stay valid.
- One read head for both channels: no stereo image drift.
- Ratio limit ±7 semitones. Formants move with the pitch (see §14).

### 6.5 Supporting pieces
- `FixedDelay`: integer dry-path delay; latency changes crossfade over 5 ms (no click).
- `Biquad`: transposed direct form II filters.
- `SpscQueue`: lock-free single-producer/single-consumer queue for UI telemetry.
- `TunerChain`: the complete real-time path above; the plugin is a thin wrapper around it.

---

## 7. User interface architecture

- The editor draws a fixed **900 × 600** design canvas and scales it uniformly (75–150 %), so the
  layout is designed once and stays vector-sharp at every size.
- Everything is custom-drawn (no stock widgets visible). Colours and fonts are tokens in
  `Source/UI/Theme.h`. Text uses the embedded **Inter** typeface so rendering never depends on fonts
  installed on the system.
- Components: `Knob`, `Orb`, `KeyboardPanel`, `SegmentedControl`, `ValueStrip`, `IconButton`,
  `PillButton`, styled menus/tooltips in `LyraLookAndFeel`.
- All controls are bound to real parameters through the framework's parameter attachments, so the
  UI, host automation and saved state always agree. No decorative controls.
- Visual data: the audio thread pushes one `Telemetry` record per detector hop (input pitch, output
  pitch, target, confidence, level) into a lock-free queue; the editor drains it at 60 Hz. When the
  editor is closed the queue simply fills and drops — the audio thread never waits.

---

## 8. Real-time safety

The audio callback never allocates, locks, logs, touches files, or waits on another thread.

- All buffers are allocated in `prepareToPlay`, sized for the lowest Range so switching Range never
  allocates.
- Parameters are read from lock-free atomics once per block.
- A unit test replaces global `operator new` and asserts **zero allocations** while the full chain
  processes audio with bypass, Range and Speed changing.
- Denormals are disabled for the callback; NaN/Inf input is sanitised.
- Mono and stereo layouts are supported (input layout must match output).
- Latency re-reporting happens on the message thread (10 Hz timer), never on the audio thread.

---

## 9. State, snapshots and undo

- Saved state format (versioned):
  `<LYRA version="1"> <STATE …parameters…/> <AB slot=".."><A/><B/></AB> <UI …display prefs…/> </LYRA>`.
  Missing or unknown pieces fall back to defaults; malformed data is ignored.
- **A/B**: two full parameter snapshots, kept with the project.
- **Undo/Redo**: bounded history (≥ 20 transactions). A knob drag, an XY drag, a key toggle or a
  Reset All is one step each.

---

## 10. Building

Lyra Tune is cross-compiled for Windows x64 on Linux. Requirements:

| Tool | Version used | Purpose |
|---|---|---|
| clang / clang-cl, lld (`lld-link`), llvm (`llvm-rc`, `llvm-lib`, `llvm-mt`) | 23.1 | compiler, linker, resource tools |
| CMake | ≥ 3.22 (4.4 used) | build system |
| Ninja | 1.13 | build tool |
| git | any | sources and submodule |
| xwin | 0.10.0 | downloads the Windows C runtime + SDK headers/libraries (you must accept their license) |
| A native C++20 compiler (gcc or clang) | — | unit tests and offline tools |
| Wine (optional) | 11.x | smoke test and running the plugin on Linux |

### One-time setup
```sh
git clone --recursive <this repository> LyraTune && cd LyraTune
# or, if already cloned: git submodule update --init

xwin --accept-license --arch x86_64 --cache-dir ~/.cache/xwin splat --output ~/.local/share/xwin
scripts/setup-xwin-aliases.sh      # adds case-matching .lib symlinks expected by the framework
```
`XWIN_DIR` overrides the SDK location (default `~/.local/share/xwin`).

### Build
```sh
scripts/build.sh            # Release -> build/win-release/LyraTune_artefacts/Release/VST3/Lyra Tune.vst3
scripts/build.sh Debug      # Debug   -> build/win-debug/...
scripts/test.sh             # native unit tests (Release)
scripts/test.sh --asan      # unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
scripts/build-smoke.sh      # optional: build/vst3_load_smoke.exe (loads the DLL under Wine)
```
Notes:
- Builds use the static C runtime (`/MT`), so the plugin needs no extra runtime DLLs.
- Builds with `-j4` by default (`JOBS=8 scripts/build.sh` to change).
- The product name is set once in `CMakeLists.txt` (`PRODUCT_NAME`); DSP code never refers to it.
- Building natively on Windows with CMake and a clang-cl toolchain should work but is untested.

---

## 11. Installing

**Windows:** copy the `Lyra Tune.vst3` folder to `C:\Program Files\Common Files\VST3\`, then rescan
plugins in your DAW.

**Linux with a Windows DAW under Wine:**
```sh
WINEPREFIX=/path/to/your/daw/prefix scripts/install.sh
```
The script refuses to overwrite the plugin while the DAW has it loaded, smoke-tests the new build in
the same prefix (if `build/vst3_load_smoke.exe` exists), then installs to
`<prefix>/drive_c/Program Files/Common Files/VST3/`. Rescan, or re-insert the plugin after updating.

A native Linux VST3 is a different binary and will not load in a Windows DAW running under Wine.

---

## 12. Testing and measurements

### Automated tests (`scripts/test.sh`) — 35 tests, all passing; ASan/UBSan clean
- Frequency ↔ MIDI conversion, reference pitch, cents error.
- Scale masks, transposition, nearest allowed note, ties, hysteresis, mask changes, empty mask.
- YIN on sines 80–880 Hz at 44.1 / 48 / 88.2 / 96 / 192 kHz: max error < 3 cents, > 99 % voiced.
- Harmonic tones, including a dominant 2nd harmonic: zero octave errors. Fundamental drop-out: no
  octave jump. Real octave leap: followed after the hold.
- White noise: ≤ 2 % false voicing. NaN/Inf bursts: recovers, outputs stay finite.
- Identical results for host block sizes 1, 7, 32, 64, 333, 1024.
- Shifter: exact delay at ratio 1; < 3 cents pitch error at ±0.3 / 1 / 6 semitones (44.1/48/96 kHz);
  no splice discontinuities on pure tones; mean delay = reported latency for all Ranges.
- Correction: Amount scaling, Speed time constant (63.2 % after one time constant), negative Speed,
  invalid values, unvoiced release.
- Note Stabilizer: excursions shorter than the threshold never retarget; confirmation lands at
  40 / 80 / 200 ms (± one 2 ms hop); onsets after silence are immediate; a 20 ms consonant gap keeps
  the stabilizer engaged.
- Sustain: measured time constant after a 1 s hold at Speed 80 ms: 80 ms (0), 10 ms (+100),
  640 ms (−100); Speed 0 stays instant.
- End to end: a tone 35 cents sharp is corrected to within 0.04 cents.
- Full chain: zero heap allocations while switching bypass/Range/Speed; no clicks (max sample step
  equals the input's own).

### Real vocal (3-minute untuned lead vocal, 48 kHz)
Measured with `Tools/analyze_vocal` on the plugin's own output (Range High, C major):

| Signal | Median error | 90th percentile | Within 10 cents | In key |
|---|---|---|---|---|
| Unprocessed | 21.1 c | 43.5 c | 25 % | 79 % |
| Speed 60 ms | 13.7 c | 39.6 c | 39 % | 87 % |
| Speed 20 ms | 9.1 c | 36.1 c | 53 % | 91 % |
| Speed 0 ms | 1.6 c | 15.1 c | 85 % | 98 % |

Note Stabilizer on the same vocal (Speed 0, Range High, C major):

| Stabilizer | Target changes / min | 90th-percentile error |
|---|---|---|
| None | 763 | 15 c |
| Short (40 ms) | 293 | 30 c |
| Mid (80 ms) | 192 | 33 c |
| Long (200 ms) | 202 | 38 c |

Fewer target changes means less note-flipping on fast runs; the higher error is by design
(unconfirmed notes are left natural). The default is None.

- End-aligning the detector window cut the Speed-0 90th percentile from 28.8 to 14.7 cents with no
  added latency.
- Octave jumps on the same vocal: 488 before octave protection, 124 after (Low range).
- Experiment: letting the detector see 2 ms ahead (2 ms extra latency) would lower the 90th
  percentile to ~9 cents — a candidate for a future high-quality mode.

### CPU
- Detector, 128-sample blocks, Low range (worst case), native Release build, 6-core/12-thread
  laptop CPU: **48 kHz avg 218 µs, max 338 µs** per block (12.7 % of the 2667 µs block budget);
  192 kHz avg 56 µs, max 186 µs.
- Full chain offline throughput: roughly 55× real time for stereo 48 kHz (≈ 2 % of one core).

### Host testing (Windows DAW under Wine, 48 kHz / 128 samples)
Verified: plugin scan and load, audio passthrough, correction on real vocals, UI rendering and
interaction. Not yet verified: project save/reload with A/B, delay compensation after a Range
change, automation of every parameter, multiple instances, window resizing inside the host.

### Tools
- `Tools/analyze_vocal` — pitch range, tuning offset, key estimate (Krumhansl–Kessler profiles),
  octave-jump count, optional per-hop CSV.
- `Tools/render_tuner` — renders audio through the exact plugin signal path, latency-aligned.

```sh
ffmpeg -v error -i vocal.wav -ac 1 -f f32le - | build/tests/analyze_vocal 48000 150
ffmpeg -v error -i vocal.wav -ac 2 -f f32le - | build/tests/render_tuner 48000 0 100 0 1 2 \
  | ffmpeg -v error -f f32le -ar 48000 -ac 2 -i - out.wav
# render_tuner args: sampleRate speedMs amount% root(0=C) scale(0 chrom,1 maj,2 min) range(0 low,1 mid,2 high)
#                   [refHz] [stabilizerMs] [sustain -100..100]   (stderr: target changes/min, stabilizer holds)
```

---

## 13. Project structure

```
CMakeLists.txt              plugin target, product name, binary assets
cmake/windows-x64-clang-cl.cmake   cross-compile toolchain
Source/
  PluginProcessor.*         host integration, parameters, latency reporting, state, A/B, undo
  PluginEditor.*            window layout, telemetry draining, header actions
  Parameters.h              stable parameter IDs, ranges, defaults
  DSP/TunerChain.*          complete real-time signal path
  DSP/Doubler.*             two-voice doubler
  DSP/Pitch/                YinDetector, TargetSelector, CorrectionEngine, PitchShifter, Scale, PitchMath
  DSP/Common/               Biquad, FixedDelay, SpscQueue
  UI/                       Theme, LookAndFeel, Knob, Orb, KeyboardPanel, Widgets
Tests/                      native unit tests (+ smoke/ Windows DLL load test)
Tools/                      analyze_vocal, render_tuner
Resources/fonts/            Inter (SIL OFL 1.1)
scripts/                    build, test, install, xwin setup, smoke build
licenses/                   third-party license texts
third_party/JUCE            plugin framework (git submodule, 9.0.4)
```

---

## 14. Known limitations

- **Formants shift with the pitch.** Fine for normal correction; corrections beyond ~3 semitones
  sound noticeably "chipmunk" or "giant". No formant preservation yet.
- Notes below the Range floor are not corrected.
- Instantaneous shifter delay varies by up to ± half a period around the reported latency.
- After a Range change the new latency is reported to the host within ~100 ms; change Range while
  stopped.
- Host automation of parameters is also recorded in the undo history.
- Windows x64 VST3 only. Other formats and native Linux/macOS builds are not set up.
- The credit link opens the system browser; under Wine this depends on the prefix's browser setup.

---

## 15. Roadmap

1. **Presets** — factory and user presets, folders, "keep my scale".
2. **Reference tone** — notes and chords at the reference pitch.
3. **Groups** — shared settings across instances.
4. Formant preservation, pitch-history view.

---

## 16. Changelog

### 0.3.1 — 2026-10-24
- Fix: the root-note and main menus closed immediately in some hosts running under Wine. Menus now
  open inside the plugin window.

### 0.3.0 — 2026-10-24
- **HQ mode** (pill beside the latency readout): the pitch detector looks 2 ms ahead of the shifter.
  Test vocal at Speed 0: 90th-percentile error 15.1 → 9.9 cents; latency +2 ms (3.6 → 5.6 ms).
- **Doubler**: two voices built from the tuned signal, each with its own slowly drifting delay and
  small detune (about −8 / +6 cents), panned by **Width** and mixed in by **Blend**. Off = output unchanged.
  The orb shows the voices as satellites.
- Interface: Scale, Stabilizer and Range sit in subtle glass sections with hairline dividers; the Doubler
  and Latency/HQ blocks are framed the same way; selected options get a soft glow.
- Fix: a freshly opened editor could start at 75 % instead of its saved size.
- 35 automated tests.

### 0.2.0 — 2026-10-23
- **Note Stabilizer** (None / Short 40 ms / Mid 80 ms / Long 200 ms): new notes must hold before they
  are tuned; runs, grace notes and scoops stay natural. Zero added latency.
- **Sustain** (−100 … +100): retune speed follows note age — tighten or loosen held notes.
- Speed knob: definitive **0 ms** notch with snap, and an animated "overload" state in the aggressive
  range; the orb ring heats up to match.
- Orb shows notes awaiting stabilizer confirmation.
- Latency readout moved to the main panel; shorter tooltips.
- 30 automated tests.

### 0.1.0 — 2026-10-23
First release.
- Real-time YIN pitch detection with octave protection, 2 ms estimates.
- Key/scale correction: Chromatic, Major, Minor, Custom; root; reference pitch.
- Speed (incl. aggressive −3…0 ms) and Amount.
- Low-latency period-synchronous pitch shifter; Range setting (3.6 / 5.3 / 7.4 ms at 48 kHz),
  reported to the host; click-free, latency-aligned bypass.
- Custom interface: live orb with XY pad, Speed/Amount knobs, keyboard with Input/Output HeatMaps
  and detected notes, undo/redo, reset, A/B snapshots, tooltips, 75–150 % scaling.
- 26 automated tests, offline analysis/render tools.

---

## 17. Credits and licenses

- **Lead design and engineering: stack**
- Development assistance: Claude (AI assistant)

Lyra Tune is free software: you can redistribute it and/or modify it under the terms of the
**GNU Affero General Public License v3** (see `LICENSE`).

Third-party components and their licenses are listed in `THIRD_PARTY_NOTICES.md`; full texts are in
`licenses/`.
