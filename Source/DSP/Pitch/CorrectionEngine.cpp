#include "CorrectionEngine.h"
#include "PitchMath.h"
#include <algorithm>
#include <cmath>

namespace lyra
{
static double onePoleAlpha (double seconds, double sr) noexcept
{
    return seconds > 0 ? 1.0 - std::exp (-1.0 / (seconds * sr)) : 1.0;
}

void CorrectionEngine::prepare (double sampleRate, double estimateIntervalSeconds)
{
    sr = sampleRate;
    dt = estimateIntervalSeconds > 0 ? estimateIntervalSeconds : 0.002;
    updateCoefficients();
    reset();
}

void CorrectionEngine::reset() noexcept
{
    selector.reset();
    desired = c = inMidi = o = 0;
    noteAge = candidateAge = 0;
    unvoicedTime = 1.0;
    active = false;
    target = confirmed = candidate = TargetSelector::kNoTarget;
    alpha = baseAlpha;
}

void CorrectionEngine::setParams (const Params& np) noexcept
{
    Params q = np;
    q.speedMs = std::isfinite (q.speedMs) ? std::clamp (q.speedMs, -3.0, 1000.0) : 20.0;
    q.amount = std::isfinite (q.amount) ? std::clamp (q.amount, 0.0, 1.0) : 1.0;
    q.referenceHz = std::isfinite (q.referenceHz) ? std::clamp (q.referenceHz, 400.0, 480.0) : 440.0;
    q.stabilizerMs = std::isfinite (q.stabilizerMs) ? std::clamp (q.stabilizerMs, 0.0, 1000.0) : 0.0;
    q.sustain = std::isfinite (q.sustain) ? std::clamp (q.sustain, -1.0, 1.0) : 0.0;
    const bool coeffs = q.speedMs != p.speedMs || q.hysteresisCents != p.hysteresisCents;
    p = q;
    selector.setMask (p.mask);
    if (coeffs)
        updateCoefficients();
}

void CorrectionEngine::updateCoefficients() noexcept
{
    baseAlpha = alpha = onePoleAlpha (p.speedMs / 1000.0, sr);
    releaseAlpha = onePoleAlpha (std::max (p.speedMs, 20.0) / 1000.0, sr);
    selector.setHysteresisCents (std::max (0.0, p.hysteresisCents));
    overshootBeta = p.speedMs < 0 ? kOvershootMax * -p.speedMs / 3.0 : 0.0;
    overshootDecay = std::exp (-1.0 / (kOvershootRelease * sr));
}

void CorrectionEngine::onEstimate (const PitchEstimate& e) noexcept
{
    active = false;
    inMidi = 0;
    const int prevConfirmed = confirmed;
    const bool voiced = e.voiced && e.confidence >= p.minConfidence && e.frequency > 0;
    if (! voiced)
    {
        unvoicedTime += dt;
        if (unvoicedTime >= kOnsetGapSeconds)
            confirmed = TargetSelector::kNoTarget; // phrase ended: the next note is an onset
        candidate = TargetSelector::kNoTarget;
        candidateAge = 0;
    }
    else
    {
        unvoicedTime = 0;
        inMidi = freqToMidi (e.frequency, p.referenceHz);
        const int n = selector.select (inMidi);
        if (n == TargetSelector::kNoTarget)
        {
            confirmed = candidate = TargetSelector::kNoTarget;
        }
        else if (p.stabilizerMs <= 0 || confirmed == TargetSelector::kNoTarget || n == confirmed)
        {
            if (n != confirmed) noteAge = 0;
            confirmed = n;
            candidate = TargetSelector::kNoTarget;
            candidateAge = 0;
            active = true;
        }
        else
        {
            if (n != candidate) { candidate = n; candidateAge = 0; }
            candidateAge += dt;
            if (candidateAge * 1000.0 >= p.stabilizerMs - 1e-9)
            {
                confirmed = n;
                candidate = TargetSelector::kNoTarget;
                candidateAge = 0;
                noteAge = 0;
                active = true;
            }
        }
    }

    target = active ? confirmed : TargetSelector::kNoTarget;
    if (active)
        noteAge += dt;
    desired = active ? (target - inMidi) * p.amount : 0.0;

    // Negative Speed: kick past the new note on a note-to-note change (not an onset after silence).
    if (overshootBeta > 0 && active && prevConfirmed != TargetSelector::kNoTarget && confirmed != prevConfirmed)
        o = std::clamp (overshootBeta * (confirmed - prevConfirmed) * p.amount, -kOvershootLimit, kOvershootLimit);

    // Sustain: the held note's age shortens (+) or lengthens (-) the Speed time constant.
    alpha = baseAlpha;
    if (p.sustain != 0.0 && p.speedMs > 0.0 && active)
    {
        const double ramp = std::min (1.0, noteAge / kSustainRampSeconds);
        const double t = std::clamp (p.speedMs * std::exp2 (-3.0 * p.sustain * ramp), 0.05, 3000.0);
        alpha = onePoleAlpha (t / 1000.0, sr);
    }
}

float CorrectionEngine::nextRatio() noexcept
{
    c += (active ? alpha : releaseAlpha) * (desired - c);
    const double total = c + o;
    o = std::abs (o) > 1e-9 ? o * overshootDecay : 0.0; // flush before it goes denormal
    return float (std::exp2 (total / 12.0));
}
} // namespace lyra
