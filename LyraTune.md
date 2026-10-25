# Lyra Tune — Documentation

**Version 0.6.7** · Real-time automatic vocal pitch correction · VST3 (Windows x64, macOS) · AU (macOS)

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
| Octave/harmonic-error protection | Unit-tested, measured |
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
| LyraStrike (formant accent on note changes) | Unit-tested, measured (not yet host-tested) |
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
- **Menu** (left): window size 75 / 100 / 125 / 150 %, HQ mode (+2 ms latency).
- **Undo / Redo / Reset All**: edit history (≥ 20 steps); Reset All is undoable.
- **A / B** with the chevron between them: switch between two complete settings snapshots; the
  chevron copies the current snapshot to the other one.
- **?**: show or hide tooltips.

**Doubler and LyraStrike (top-left)** — the Doubler block (power button, Width, Blend), and below
it the **LyraStrike** toggle (bolt icon) with its **Amount** and **Length** strips. LyraStrike makes every
note change hit harder: for a short moment the formants dip on a step up (darker, larger) or rise on a step down
(brighter, smaller), then return. Amount sets how far: ±1 % (subtle) … ±25 % (extreme), default ±7.5 %;
double-click resets it. Length sets how long each accent lasts: 25 ms (tight) … 100 ms (a sweeping
tail), default 80 ms; the 3 ms hit is the same at every length, only the return is stretched. The tuned pitch is not touched and no latency is added. When on, the toggle and its
frame glow red; off, no glow. The first note of a phrase (after ≥ 60 ms of silence) gets no accent.

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
- LyraStrike accents every target change, including fast flips on runs; pair it with the Stabilizer
  if busy passages get too much bite.
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
| `strike` | LyraStrike | off / on | off | yes |
| `strikeAmt` | LyraStrike Amount | ±1 … ±25 % (skewed toward small values) | ±7.5 % | yes |
| `strikeLen` | LyraStrike Length | 25 … 100 ms | 80 ms | yes |

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
- Octave/harmonic protection: (1) prefer twice the period when the half-period dip is much shallower than the
  full one; (2) a 30 ms continuity hold keeps the previous period when the estimate jumps to a
  harmonic (period ÷ 2…6 or × 2/3) or subharmonic (× 2, × 3) but the old period is still valid. On
  real vocals these jumps (12/19/24/28 semitones, 4–40 ms) were the bulk of detector errors; widening
  the hold from octaves to harmonics cut A→B→A target flickers by 20–40 % (§12). Holding the lower octave is safe for the shifter (a
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
- Requested correction = `(target − pitch) · Amount`, in semitones.
- **Prediction.** An estimate describes audio ~1–2 ms older than the audio the shifter is playing
  (window centre minus shifter delay). Every sample uses the pitch extrapolated along the detector's
  own slope: `p(t) = m + v · (lead + time since estimate)`, `v = Δm / hop`,
  `lead = (window + 1/f)/2 + 0.1 ms − shifter delay`. No added latency. The slope is capped at
  100 st/s and the predicted offset at ±20 c, so one bad estimate cannot predict a wrong note; jumps
  > 0.75 st and onsets reset it. This removes the lag error HQ mode used to fix by buffering, and the
  2 ms correction staircase. (Smoothing the detected pitch was tried and measured worse: real voice
  movement exceeds the detector noise.)
- **Smoothing.** The **correction offset** (not the output pitch) is smoothed by two cascaded
  one-poles (critically damped), each `τ = Speed / 2.146`, so the step response reaches 63.2 % at
  `t = Speed` like a one-pole, but starts with zero slope and never overshoots: retunes glide in an
  S-curve. Speed ≤ 0 never reaches the exponential: 0 → instant; −3…0 → instant plus reduced hysteresis.
- **Vibrato analyser.** A zigzag detector confirms a pitch peak/trough once the pitch turns back by
  8 c. The last three extrema count as vibrato when both half-cycles last 35–170 ms (≈ 3–14 Hz) and
  both swings are 16–200 c and within 1.5× of each other — a glide or note change never qualifies.
  Depth `A = min(swing)/2`, centre `= (e0 + 2·e1 + e2)/4`. Ignored when the pitch leaves
  `centre ± (1.5·A + 10 c)`. Used for:
  - **Hysteresis** `h = min(15 c + A/2, 45 c)`: wide vibrato around a note sung sharp or flat no
    longer flips the target every cycle.
  - **Vibrato kept at Speed > 0**: the pitch to correct is `centre + F·(p − centre)`,
    `F = exp(−Speed / 12 ms)`. The vibrato's centre is tuned with Speed; its shape passes untouched.
    Synthetic ±40 c / 5.5 Hz vibrato 30 c sharp, kept in phase: 20 ms 32 % → 86 %, 40 ms 66 % → 99 %
    (before: slow Speed kept vibrato only by lagging behind it, which reshaped it); centre error 0 c.
- **Onsets.** The first note of a phrase that starts steady (smoothed slope < 6 st/s) is tuned with
  `min(Speed, 8 ms)` for its first 40 ms, instead of gliding in from its untuned start; a note that
  starts with a scoop keeps Speed, so the scoop stays natural. Steady onset 30 c sharp, error after
  30 ms: 1.8–21 c → 0.2–0.3 c across Speed 10–80 ms.
- Unvoiced or low-confidence (< 0.70) frames release the correction to zero with
  `max(Speed, 20 ms)`.
- shortcut: the vibrato keep constant (12 ms), turn threshold (8 c), 45 c cap and onset values are
  first estimates from three vocals and synthetic tests; tune them by ear.

**Note Stabilizer** (causal, zero added latency). When the selected note differs from the confirmed
target it becomes a *candidate*. While unconfirmed, the correction releases toward zero, so the
natural pitch passes. After the candidate persists for the threshold (40 / 80 / 200 ms) it is
confirmed and tuned. A note starting after ≥ 60 ms of unvoiced input is confirmed immediately, so
only note-to-note transitions wait; short consonant gaps inside a phrase keep the stabilizer engaged.
A lookahead design would delay all audio by the threshold; this one never delays anything, so it stays
available at every Range setting.

**Sustain** tracks the age of the confirmed note and scales Speed's time constant:
`T_eff = T · 2^(−3 · sustain · min(1, age / 0.5 s))` with sustain in −1…1. At +100 a note held for
0.5 s retunes 8× faster; at −100, 8× slower. Parameter changes act through the smoother, so
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

### 6.5 LyraStrike — `FormantAccent`
A formant shift with the pitch untouched, computed causally (no added latency):
- `e = A(z)·x` whitens the tuned signal with its own spectral envelope (LPC, FIR lattice);
  `y = g · e / A_s(z)` re-colours it with the envelope of the **same 20 ms window read `s` times
  faster** (its spectrum, and so its formants, scale by `s`; IIR lattice). `g = √(E_s/E)` holds the
  level. The harmonics stay in `e` and are never resampled, so the pitch cannot move.
- Accent: `u` ramps to ±1 over 3 ms (raised cosine) and back to 0 over `Length − 3 ms` (Length 25–100 ms,
  default 80; latched at each trigger); `s = 1 + depth·u`, depth = Amount (1–25 %, default 7.5 %), read
  at every coefficient update. Measured active time per accent: Length + 0–1 ms; peak at 3.0 ms.
  On real vocals ~40 % of accents are restarted by the next note change and chain into longer bursts
  (at 25 ms: mean 30–36 ms, max 188 ms on busy runs).
  Step up → `u = −1`; step down → `u = +1`. Retriggers ramp from the current `u`.
- Trigger: a change of the target note from one note to another. A pending (Stabilizer) note or a
  short consonant gap keeps the previous note; ≥ 60 ms unvoiced starts a new phrase (no accent).
- LPC order 16 per 48 kHz (max 32), Gaussian lag window 100 Hz, −40 dB noise floor; coefficients
  every 2 ms, reflection coefficients interpolated per sample (stable). Autocorrelation is strided at
  high sample rates so each update costs the same at any rate.
- Seamless: with `k_s = k` and equal lattice states the cascade is an exact identity, so accents fade
  in and out without a seam, and between accents the audio passes untouched (bit-exact, no CPU).
- Sits after the pitch shifter and before the Doubler (the doubled voices carry the accent).
- Measured (synthetic 3-formant vowels, f0 110/220 Hz, 44.1–96 kHz): the formant region moves
  **68–91 % of a true 4 % move**, in the right direction; pitch averaged over the accent unchanged
  (< 0.25 c); the detector sees a transient ≤ 1.9 c while the waveform shape changes. Level within
  0.03 dB; no new clicks. An LPC envelope is smoother than a real vocal tract's, so sharp formant
  peaks move less than an ideal 4 % shift (per-harmonic detail ≈ 25–45 % of ideal).
- Amount (48 kHz, same vowels): move vs a true shift of that size ±1 % 78–84 %, ±4 % 78–86 %,
  ±10 % 89–97 %, ±25 % 124–138 % (a little past the target). Pitch averaged over the accent stays
  within 0.25 c at every amount; the brief detector transient grows with the amount (0.5 / 1.7 / 4.4 /
  12.8 c). At ±25 % the level rises 0.3 dB during accents and the sharpest sample step ~20 %.
- shortcut: order, lag window and window length were chosen on synthetic vowels; revisit by ear on
  very high or very low voices.

### 6.6 Supporting pieces
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

### Automated tests (`scripts/test.sh`) — 45 tests, all passing; ASan/UBSan clean
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
- LyraStrike: formant move vs a true 4 % move, direction, pitch kept (44.1/48/96 kHz); Amount 1–25 %
  scales the move monotonically with pitch kept; Length 25/50/100 ms gives that active time with the
  peak at 3 ms; no clicks (4 % and 25 %, every length), level kept, bit-exact passthrough between accents, accent ends; in the chain, accents fire only on
  note-to-note changes (not phrase onsets) with the right direction, no allocations.
- Full chain: zero heap allocations while switching bypass/Range/Speed; no clicks (max sample step
  equals the input's own).

### 0.5.0 tuning algorithm (three vocal stems, 48 kHz, `Tools/render_tuner` → `Tools/analyze_vocal`)
Material: FL Studio demo stems (likely already produced): a 15 s dry vocal (D minor, Range High) and
two 3-minute leads (E major and C# major, Range Low). Output is re-analysed with the same detector,
so ~1 c of the numbers is measurement floor. "Centre" = mean of 100 ms single-note windows.

| Speed 0 (HQ off) | 90th-pct error to nearest note: 0.4.1 → 0.5.0 | Note-centre p90 |
|---|---|---|
| Dry 15 s | 8.5 → 6.0 c | 1.0 c |
| Lead E major | 13.6 → 11.9 c | 1.4 c |
| Lead C# major | 20.0 → 16.9 c | 3.6 c |

0.5.0 without HQ beats 0.4.1 with HQ on all three (HQ: 9.0 / 12.5 / 18.3 c). A→B→A target
flickers (< 60 ms) on the two leads: 341 → 212 and 224 → 178 per minute (harmonic hold); the rest
are fast runs crossing scale gaps, which the Note Stabilizer is for. At Speed 20/60 the note-centre
error moves by ±1–2 c (mostly up): part is vibrato now kept on purpose (100 ms windows do not
average a 5 Hz cycle out), part is the S-curve landing slightly later. Judge these settings by ear.
CPU: unchanged (full chain, Low range: 13.68 s → 13.79 s for 211 s of stereo audio).
HQ on top of 0.5.0 (Speed 0, p90): 6.0 → 6.1 / 11.9 → 11.6 / 16.9 → 15.5 c — a small remaining gain for
+2 ms; most of what HQ bought is now done by prediction.

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
- Full chain offline throughput (stereo 48 kHz, 211 s vocal): Low range ~15× real time, High range
  ~60×. LyraStrike on: +2 s for 211 s (~1 % of a core on average; it only works during accents).
- LyraStrike worst case (retriggered continuously, 128-sample stereo blocks, measured with a DAW
  running on the same machine): 48 kHz avg 112 µs / max ~210–470 µs (budget 2667 µs); 192 kHz avg 93 µs,
  max ~310–730 µs (budget 667 µs). At 192 kHz with 128-sample buffers, the detector's and
  LyraStrike's worst cases together can approach the block budget: use ≥ 256-sample buffers there.

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
  DSP/FormantAccent.*       LyraStrike formant accent
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
- Formats: VST3 for Windows x64; VST3 and AU for macOS 11+ (universal). The macOS build is built and
  validated on GitHub Actions (auval, pluginval strictness 5) but not yet tested in a DAW on real
  hardware, and it is not notarized (users must clear the quarantine flag). No native Linux build.
- The credit link opens the system browser; under Wine this depends on the prefix's browser setup.

---

## 15. Roadmap

1. **Presets** — factory and user presets, folders, "keep my scale".
2. **Reference tone** — notes and chords at the reference pitch.
3. **Groups** — shared settings across instances.
4. Formant preservation, pitch-history view.

---

## 16. Changelog

### 0.6.7 — 2026-10-24
- **macOS build**: VST3 and Audio Unit, universal (Apple Silicon + Intel), macOS 11 or later. Built,
  ad-hoc signed and validated (auval, pluginval strictness 5) by `.github/workflows/macos.yml`. Not
  notarized; not yet tested in a DAW on real hardware.
- No DSP or sound changes; Windows build unchanged apart from the version number.

### 0.6.6 — 2026-10-24
- LyraStrike defaults are now **Amount ±7.5 %, Length 80 ms** (were ±4 % / 25 ms). Projects saved
  with 0.6.5 keep their own values; projects saved with 0.6.0 (no Amount/Length yet) are migrated to
  ±4 % / 25 ms on load so they sound as they did.

### 0.6.5 — 2026-10-24
- **LyraStrike Amount**: formant shift from ±1 % to ±25 % (default ±4 %, the 0.6.0 sound), on a red
  strip under the LyraStrike toggle. New parameter `strikeAmt`; projects from 0.6.0 load at ±4 %.
- **LyraStrike Length**: 25–100 ms per accent (default 25 ms, the 0.6.0 sound). The 3 ms hit stays;
  only the return is stretched. New parameter `strikeLen`.
- The LyraStrike section glow follows the section's shape.
- 45 automated tests.

### 0.6.0 — 2026-10-24
- **LyraStrike**: a 25 ms formant accent on every note change (±4 %: darker going up, brighter going
  down), with the tuned pitch untouched and no added latency. Toggle below the Doubler; glows red
  when on. New parameter `strike` (default off; older projects load with it off).
- Fix: the 0.5.0 notes said 41 tests; there were 40. Now 43.

### 0.5.0 — 2026-10-24
New tuning algorithm (no added latency, no new parameters; projects will sound different):
- Pitch prediction: corrections follow the voice without the detector's 1–2 ms lag. Speed 0 is
  tighter than 0.4's HQ mode without HQ's extra latency.
- Harmonic hold: detector jumps to the 2nd–6th harmonic no longer flip the target note.
- Speed glides in an S-curve (critically damped) instead of a one-pole.
- Vibrato: hysteresis widens with vibrato depth; slow Speeds tune the vibrato's centre and keep its shape.
- Steady phrase onsets are tuned within ~8 ms at any Speed; scoops stay natural.
- Reverted the 0.4.0 negative-Speed overshoot: it kicked up to a full semitone past each new note,
  which was heard as wrong notes. Negative Speed is back to the 0.3.1 behaviour (hysteresis shrinks to 0).
- HQ mode moved from the Latency panel into the main menu (☰).
- 41 automated tests.

### 0.4.0 — 2026-10-24
- Negative Speed (−3…0 ms) overshot past each new note and settled over ~100 ms. Reverted in 0.5.0.

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
