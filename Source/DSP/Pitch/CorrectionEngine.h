#pragma once
#include "TargetSelector.h"
#include "YinDetector.h"

namespace lyra
{
// Turns pitch estimates into a per-sample pitch ratio: target selection -> note stabilizer ->
// requested correction (semitones, scaled by Amount) -> one-pole smoothing with the Speed time
// constant (modulated by Sustain).
//
// Smoothing the correction offset (not the output pitch) is what makes slow Speed natural: fast input
// movement such as vibrato passes through, because the offset cannot follow it.
//
// Speed mapping (ms):
//   > 0  : one-pole time constant T = speed; alpha = 1 - exp(-1 / (T * fs)).
//   == 0 : instant (alpha = 1): correction jumps at each detector hop (2 ms).
//   < 0  : "aggressive" (-3..0): instant, and target hysteresis shrinks linearly to 0 at -3, so
//          the output snaps between notes like a hard-tuned effect. Negative values never reach exp().
//
// Note Stabilizer (causal, adds no latency): when the selected note differs from the confirmed
// target it becomes a candidate. While unconfirmed, the correction releases toward 0 so the natural
// pitch passes (short runs, grace notes and scoops are not snapped to a wrong note). After the
// candidate persists for `stabilizerMs` it is confirmed. A note starting after >= 60 ms of unvoiced
// input is confirmed immediately: only note-to-note transitions wait.
//
// Sustain (-1..1) scales Speed by note age: T_eff = T * 2^(-3 * sustain * min(1, age / 0.5 s)),
// i.e. up to 8x faster (+1) or slower (-1) once a note has been held for 0.5 s. No effect when
// Speed <= 0 (already instant).
//
// Unvoiced / pending / no allowed notes: correction releases to 0 with max(T, 20 ms).
class CorrectionEngine
{
public:
    struct Params
    {
        double speedMs = 20.0;
        double amount = 1.0;       // 0..1
        double referenceHz = 440.0;
        PitchClassMask mask = kAllNotes;
        double hysteresisCents = 15.0;
        float minConfidence = 0.70f;
        double stabilizerMs = 0.0; // 0 = off
        double sustain = 0.0;      // -1..1
    };

    static constexpr double kSustainRampSeconds = 0.5;
    static constexpr double kOnsetGapSeconds = 0.060;

    // estimateInterval: time between onEstimate() calls (the detector hop).
    void prepare (double sampleRate, double estimateIntervalSeconds = 0.002);
    void reset() noexcept;
    void setParams (const Params&) noexcept; // per block; cheap when unchanged
    void onEstimate (const PitchEstimate&) noexcept;
    float nextRatio() noexcept;

    // Diagnostics (audio-thread values; copy out through a queue for the UI).
    double inputMidi() const noexcept { return inMidi; }      // 0 when unvoiced
    int targetNote() const noexcept { return target; }        // TargetSelector::kNoTarget when none
    int pendingNote() const noexcept { return candidate; }    // stabilizer candidate, or kNoTarget
    double noteAgeSeconds() const noexcept { return noteAge; }
    double correctionSemitones() const noexcept { return c; }

private:
    void updateCoefficients() noexcept;

    Params p;
    TargetSelector selector;
    double sr = 48000, dt = 0.002, baseAlpha = 1, alpha = 1, releaseAlpha = 1;
    double desired = 0, c = 0, inMidi = 0;
    double noteAge = 0, candidateAge = 0, unvoicedTime = 1.0;
    bool active = false;
    int target = TargetSelector::kNoTarget, confirmed = TargetSelector::kNoTarget, candidate = TargetSelector::kNoTarget;
};
} // namespace lyra
