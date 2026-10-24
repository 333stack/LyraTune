#include "TestHarness.h"
#include "Pitch/PitchMath.h"
#include "Pitch/Scale.h"
#include "Pitch/TargetSelector.h"
#include "Pitch/YinDetector.h"
#include "Pitch/PitchShifter.h"
#include "Pitch/CorrectionEngine.h"
#include "Common/FixedDelay.h"
#include "TunerChain.h"
#include "Doubler.h"
#include <atomic>
#include <new>

#include <algorithm>
#include <chrono>
#include <limits>
#include <random>

using namespace lyra;

// Global allocation counter: lets tests assert the real-time path never allocates.
// (malloc/free pairs are correct here; GCC's mismatched-new-delete check cannot see that.)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
static std::atomic<long> gAllocs { 0 };
void* operator new (size_t n) { ++gAllocs; if (void* p = std::malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[] (size_t n) { ++gAllocs; if (void* p = std::malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, size_t) noexcept { std::free (p); }
void operator delete[] (void* p, size_t) noexcept { std::free (p); }
constexpr double kPi = 3.14159265358979323846;

// ---------- pitch math ----------
TEST (freq_midi_roundtrip)
{
    CHECK_NEAR (freqToMidi (440.0), 69.0, 1e-12);
    CHECK_NEAR (freqToMidi (261.6255653), 60.0, 1e-6);
    CHECK_NEAR (freqToMidi (880.0), 81.0, 1e-12);
    CHECK_NEAR (midiToFreq (57.0), 220.0, 1e-9);
    for (double m = 20; m < 110; m += 0.37)
        CHECK_NEAR (freqToMidi (midiToFreq (m)), m, 1e-9);
}

TEST (reference_frequency)
{
    CHECK_NEAR (freqToMidi (432.0, 432.0), 69.0, 1e-12);
    CHECK_NEAR (freqToMidi (440.0, 432.0), 69.0 + 12.0 * std::log2 (440.0 / 432.0), 1e-12);
    CHECK_NEAR (midiToFreq (69.0, 442.0), 442.0, 1e-9);
}

TEST (cents_error)
{
    CHECK_NEAR (centsError (60.25, 60.0), -25.0, 1e-9);
    CHECK_NEAR (centsError (59.9, 60.0), 10.0, 1e-9);
    CHECK_NEAR (centsError (freqToMidi (445.0), 69.0), -1200.0 * std::log2 (445.0 / 440.0), 1e-9);
}

// ---------- scale masks ----------
TEST (scale_masks)
{
    CHECK (scaleMask (ScaleType::Chromatic, 5) == kAllNotes);
    const PitchClassMask cMaj = scaleMask (ScaleType::Major, 0);
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) CHECK (isAllowed (cMaj, pc));
    for (int pc : { 1, 3, 6, 8, 10 })       CHECK (! isAllowed (cMaj, pc));
    // A minor == C major (relative keys); G major contains F#, not F.
    CHECK (scaleMask (ScaleType::Minor, 9) == cMaj);
    const PitchClassMask gMaj = scaleMask (ScaleType::Major, 7);
    CHECK (isAllowed (gMaj, 6) && ! isAllowed (gMaj, 5));
    CHECK (transposeMask (cMaj, 12) == cMaj && transposeMask (cMaj, -5) == transposeMask (cMaj, 7));
    CHECK (scaleMask (ScaleType::Custom, 3, 0xF000 | 0x0001) == 0x0001); // stray high bits dropped
}

// ---------- target selection ----------
TEST (nearest_allowed)
{
    const PitchClassMask cMaj = scaleMask (ScaleType::Major, 0);
    CHECK (TargetSelector::nearestAllowed (60.3, kAllNotes) == 60);
    CHECK (TargetSelector::nearestAllowed (60.7, kAllNotes) == 61);
    CHECK (TargetSelector::nearestAllowed (60.9, cMaj) == 60); // flat C# -> C
    CHECK (TargetSelector::nearestAllowed (61.2, cMaj) == 62); // sharp C# -> D
    CHECK (TargetSelector::nearestAllowed (61.0, cMaj) == 60); // exact tie -> lower
    CHECK (TargetSelector::nearestAllowed (66.0, PitchClassMask (1u << 0)) == 60); // only C: tritone tie -> lower
    CHECK (TargetSelector::nearestAllowed (66.4, PitchClassMask (1u << 0)) == 72);
    CHECK (TargetSelector::nearestAllowed (60.0, 0) == TargetSelector::kNoTarget);
    CHECK (TargetSelector::nearestAllowed (std::numeric_limits<double>::quiet_NaN(), kAllNotes) == TargetSelector::kNoTarget);
}

TEST (hysteresis)
{
    TargetSelector s;
    s.setHysteresisCents (15);
    CHECK (s.select (60.40) == 60);
    CHECK (s.select (60.55) == 60); // 5 c past the midpoint: inside the 15 c margin
    CHECK (s.select (60.64) == 60); // 14 c past
    CHECK (s.select (60.70) == 61); // 20 c past -> switch
    CHECK (s.select (60.40) == 61); // sticks to 61 on the way back
    CHECK (s.select (60.30) == 60);
    // Dithering around the boundary must not flip every estimate.
    int flips = 0, prev = s.select (60.5);
    for (int i = 0; i < 100; ++i) { const int t = s.select (60.5 + ((i & 1) ? 0.05 : -0.05)); flips += t != prev; prev = t; }
    CHECK (flips == 0);
}

TEST (mask_change_and_empty_mask)
{
    TargetSelector s;
    CHECK (s.select (60.9) == 61);
    s.setMask (scaleMask (ScaleType::Major, 0)); // C# no longer allowed: must leave it immediately
    CHECK (s.select (60.9) == 60);
    s.setMask (0);
    CHECK (s.select (60.9) == TargetSelector::kNoTarget);
    s.setMask (kAllNotes);
    CHECK (s.select (60.9) == 61);
}

// ---------- YIN ----------
struct Run { std::vector<PitchEstimate> est; };

template <typename Gen>
static Run runDetector (double sr, double seconds, Gen&& gen, int block = 128, YinDetector::Config cfg = {})
{
    YinDetector d;
    d.prepare (sr, cfg);
    std::vector<float> buf (static_cast<size_t> (block));
    Run r;
    const int total = int (sr * seconds);
    for (int n = 0; n < total; n += block)
    {
        const int len = std::min (block, total - n);
        for (int i = 0; i < len; ++i) buf[size_t (i)] = gen (n + i);
        d.process (buf.data(), len, [&] (int, const PitchEstimate& e) { r.est.push_back (e); });
    }
    return r;
}

// Cents error statistics over voiced estimates after a warm-up.
static void pitchStats (const Run& r, double trueHz, double& maxAbsCents, double& voicedFrac, size_t skip)
{
    maxAbsCents = 0;
    size_t voiced = 0, n = 0;
    for (size_t i = skip; i < r.est.size(); ++i, ++n)
        if (r.est[i].voiced)
        {
            ++voiced;
            maxAbsCents = std::max (maxAbsCents, std::abs (1200.0 * std::log2 (r.est[i].frequency / trueHz)));
        }
    voicedFrac = n ? double (voiced) / double (n) : 0;
}

TEST (yin_sine_accuracy_across_rates)
{
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (double f : { 80.0, 110.0, 196.0, 261.63, 440.0, 659.25, 880.0 })
        {
            auto r = runDetector (sr, 0.4, [&] (int n) { return float (0.5 * std::sin (2 * kPi * f * n / sr)); });
            YinDetector probe; probe.prepare (sr, {});
            double maxC, vf;
            pitchStats (r, f, maxC, vf, size_t (0.08 * sr / probe.hopSamples())); // skip 80 ms: frame fill + HP settling
            if (maxC > 3.0 || vf < 0.99)
                std::printf ("  sr=%g f=%g maxCents=%.3f voiced=%.2f\n", sr, f, maxC, vf);
            CHECK (maxC < 3.0);
            CHECK (vf > 0.99);
        }
}

// Vocal-like harmonic tone; optionally with the 2nd harmonic dominating (classic octave-up trap).
static float harmonicTone (int n, double sr, double f, bool strongSecond)
{
    double s = 0;
    for (int h = 1; h <= 12; ++h)
    {
        double a = 1.0 / h;
        if (strongSecond) a = h == 2 ? 1.0 : (h == 1 ? 0.25 : a * 0.5);
        s += a * std::sin (2 * kPi * f * h * n / sr + 0.3 * h);
    }
    return float (0.25 * s);
}

TEST (yin_harmonic_no_octave_errors)
{
    for (bool strong : { false, true })
        for (double f : { 98.0, 147.0, 220.0, 330.0, 523.25 })
        {
            auto r = runDetector (48000, 0.4, [&] (int n) { return harmonicTone (n, 48000, f, strong); });
            int octaveErrors = 0, voiced = 0;
            for (size_t i = 10; i < r.est.size(); ++i)
                if (r.est[i].voiced)
                {
                    ++voiced;
                    octaveErrors += std::abs (1200.0 * std::log2 (r.est[i].frequency / f)) > 600.0;
                }
            if (octaveErrors) std::printf ("  f=%g strong2nd=%d octaveErrors=%d/%d\n", f, strong, octaveErrors, voiced);
            CHECK (voiced > 0);
            CHECK (octaveErrors == 0);
        }
}

TEST (yin_silence_noise_and_nonfinite)
{
    auto silent = runDetector (48000, 0.3, [] (int) { return 0.0f; });
    CHECK (std::none_of (silent.est.begin(), silent.est.end(), [] (auto& e) { return e.voiced; }));

    std::mt19937 rng (1);
    std::normal_distribution<float> g (0.0f, 0.2f);
    auto noise = runDetector (48000, 0.5, [&] (int) { return g (rng); });
    const auto voicedNoise = std::count_if (noise.est.begin(), noise.est.end(), [] (auto& e) { return e.voiced; });
    if (voicedNoise) std::printf ("  noise voiced %ld/%zu\n", long (voicedNoise), noise.est.size());
    CHECK (voicedNoise <= long (noise.est.size() / 50)); // <= 2% false voicing on white noise

    // NaN/inf bursts must not poison state: pitch is recovered once the frame has refilled.
    const double f = 220;
    auto r = runDetector (48000, 0.6, [&] (int n) {
        if (n > 4800 && n < 4900) return (n & 1) ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
        return float (0.5 * std::sin (2 * kPi * f * n / 48000)); });
    CHECK (r.est.back().voiced);
    CHECK_NEAR (r.est.back().frequency, f, 0.5);
    for (auto& e : r.est) CHECK (std::isfinite (e.frequency) && std::isfinite (e.confidence));
}

TEST (yin_block_size_independent)
{
    auto gen = [] (int n) { return harmonicTone (n, 48000, 187.0, false); };
    auto ref = runDetector (48000, 0.3, gen, 1);
    for (int block : { 7, 32, 64, 333, 1024 })
    {
        auto r = runDetector (48000, 0.3, gen, block);
        CHECK (r.est.size() == ref.est.size());
        bool same = r.est.size() == ref.est.size();
        for (size_t i = 0; same && i < r.est.size(); ++i)
            same = r.est[i].frequency == ref.est[i].frequency && r.est[i].voiced == ref.est[i].voiced;
        CHECK (same);
    }
}

TEST (yin_octave_hold_and_genuine_leap)
{
    const double sr = 48000, f = 196.0;
    // Fundamental drops out for 20 ms (dominant 2nd harmonic): estimates must not jump an octave up.
    auto gen = [&] (int n) {
        const double t = n / sr;
        const double a1 = (t > 0.30 && t < 0.32) ? 0.02 : 0.6;
        double s = a1 * std::sin (2 * kPi * f * t);
        for (int h = 2; h <= 8; ++h) s += (h == 2 ? 0.8 : 0.3 / h) * std::sin (2 * kPi * f * h * t + h);
        return float (0.4 * s); };
    auto r = runDetector (sr, 0.5, gen);
    int up = 0;
    for (size_t i = 10; i < r.est.size(); ++i) up += r.est[i].voiced && r.est[i].frequency > f * 1.5;
    CHECK (up == 0);
    // A real octave leap (196 -> 392 Hz) must be followed after the hold expires.
    auto leap = runDetector (sr, 0.6, [&] (int n) { return harmonicTone (n, sr, n < sr * 0.3 ? f : 2 * f, false); });
    YinDetector probe; probe.prepare (sr, {});
    const size_t after = size_t ((0.3 + 0.03 + 0.04) * sr / probe.hopSamples()) + 1; // leap + hold + frame
    for (size_t i = after; i < leap.est.size(); ++i) CHECK_NEAR (leap.est[i].frequency, 2 * f, 1.0);
}

TEST (yin_tracks_note_step)
{
    // 220 Hz -> 247 Hz step at 0.25 s: detector should settle on the new note within one frame + hop.
    const double sr = 48000;
    auto r = runDetector (sr, 0.5, [&] (int n) { return harmonicTone (n, sr, n < sr / 4 ? 220.0 : 246.94, false); });
    YinDetector d; d.prepare (sr, {});
    const double settleSec = (d.frameLengthSamples() / d.detectorSampleRate()) + 2 * d.hopSamples() / sr;
    const size_t settledIdx = size_t ((0.25 + settleSec) * sr / d.hopSamples()) + 1;
    for (size_t i = settledIdx; i < r.est.size(); ++i)
        CHECK_NEAR (1200.0 * std::log2 (r.est[i].frequency / 246.94), 0.0, 5.0);
    std::printf ("  frame=%.1f ms hop=%.1f ms settle bound=%.1f ms\n",
                 1000.0 * d.frameLengthSamples() / d.detectorSampleRate(), 1000.0 * d.hopSamples() / sr, 1000 * settleSec);
}

TEST (yin_cpu_per_hop)
{
    // Not a pass/fail test: reports worst/avg analysis cost for the CPU budget (Release build only meaningful).
    for (double sr : { 48000.0, 192000.0 })
    {
        YinDetector d; d.prepare (sr, {});
        std::vector<float> blk (128);
        std::vector<double> perBlockUs;
        int n = 0;
        for (int b = 0; b < int (sr * 2 / 128); ++b)
        {
            for (auto& s : blk) s = harmonicTone (n++, sr, 150, false);
            const auto t0 = std::chrono::steady_clock::now();
            d.process (blk.data(), 128, [] (int, const PitchEstimate&) {});
            perBlockUs.push_back (std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count());
        }
        std::sort (perBlockUs.begin(), perBlockUs.end());
        double avg = 0; for (double v : perBlockUs) avg += v; avg /= double (perBlockUs.size());
        std::printf ("  sr=%g block=128 (%.0f us budget): avg %.1f us, p99 %.1f us, max %.1f us\n", sr, 128e6 / sr, avg,
                     perBlockUs[perBlockUs.size() * 99 / 100], perBlockUs.back());
    }
}

// ---------- pitch shifter ----------
// Runs `in` through the shifter at a constant ratio with the true period supplied.
static std::vector<float> shift (const std::vector<float>& in, double sr, float ratio, double periodSamples, int block = 64)
{
    PitchShifter ps;
    ps.prepare (sr, 1, 70.0);
    ps.setPeriod (periodSamples);
    std::vector<float> out = in, r (size_t (block), ratio);
    for (size_t n = 0; n < out.size(); n += size_t (block))
    {
        float* p = out.data() + n;
        ps.process (&p, 1, int (std::min (size_t (block), out.size() - n)), r.data());
    }
    return out;
}

static std::vector<float> tone (double sr, double f, double seconds, bool harmonic)
{
    std::vector<float> v (size_t (sr * seconds));
    for (size_t n = 0; n < v.size(); ++n)
        v[n] = harmonic ? harmonicTone (int (n), sr, f, false) : float (0.5 * std::sin (2 * kPi * f * double (n) / sr));
    return v;
}

static double medianPitch (const std::vector<float>& x, double sr, size_t skipEstimates)
{
    YinDetector d; d.prepare (sr, {});
    std::vector<float> f;
    d.process (x.data(), int (x.size()), [&] (int, const PitchEstimate& e) { if (e.voiced) f.push_back (e.frequency); });
    if (f.size() <= skipEstimates) return 0;
    f.erase (f.begin(), f.begin() + long (skipEstimates));
    std::nth_element (f.begin(), f.begin() + long (f.size() / 2), f.end());
    return f[f.size() / 2];
}

TEST (shifter_unity_is_pure_delay)
{
    const double sr = 48000;
    PitchShifter ps; ps.prepare (sr, 1, 70.0);
    auto in = tone (sr, 210.0, 0.3, true);
    auto out = shift (in, sr, 1.0f, sr / 210.0);
    const size_t L = size_t (ps.latencySamples());
    double maxErr = 0;
    for (size_t n = L; n < in.size(); ++n) maxErr = std::max (maxErr, double (std::abs (out[n] - in[n - L])));
    CHECK (maxErr < 1e-6);
    std::printf ("  latency %zu samples = %.2f ms @48k\n", L, 1000.0 * double (L) / sr);
}

TEST (shifter_mean_delay_equals_latency)
{
    // Across ratios and pitches the delay sawtooth must stay centred on the reported latency.
    const double sr = 48000;
    for (double minF : { 70.0, 100.0, 150.0 })
    {
        PitchShifter ps; ps.prepare (sr, 1, 70.0); ps.setMinFreq (minF); ps.reset();
        std::printf ("  range %g Hz: latency %d samples = %.2f ms\n", minF, ps.latencySamples(), 1000.0 * ps.latencySamples() / sr);
        for (double semis : { -2.0, -0.3, 0.3, 2.0 })
            for (double f : { minF * 1.01, 300.0 })
            {
                ps.reset(); ps.setPeriod (sr / f);
                std::vector<float> x (1, 0.0f), r (1, float (std::exp2 (semis / 12.0)));
                double sum = 0, lo = 1e9, hi = -1e9; const int N = 48000;
                for (int i = 0; i < N; ++i) { float* p = x.data(); ps.process (&p, 1, 1, r.data()); sum += ps.currentDelay(); lo = std::min (lo, ps.currentDelay()); hi = std::max (hi, ps.currentDelay()); }
                const double mean = sum / N, P = sr / f;
                CHECK_NEAR (mean, ps.latencySamples(), 0.06 * P + 2);
                CHECK (lo >= 2.0);
                CHECK (hi - lo <= P + 2 * 192 * 0.13 + 2); // sawtooth spans one period (+ fade drift)
            }
    }
}

TEST (shifter_pitch_accuracy)
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double semis : { -6.0, -1.0, -0.3, 0.3, 1.0, 6.0 })
            for (double f : { 110.0, 220.0, 440.0 })
            {
                const auto ratio = float (std::exp2 (semis / 12.0));
                auto out = shift (tone (sr, f, 0.6, true), sr, ratio, sr / f);
                const double got = medianPitch (out, sr, 20);
                const double errC = 1200.0 * std::log2 (got / (f * ratio));
                if (std::abs (errC) > 3.0) std::printf ("  sr=%g f=%g semis=%g err=%.2f c\n", sr, f, semis, errC);
                CHECK (std::abs (errC) < 3.0);
            }
}

TEST (shifter_splices_are_click_free)
{
    // Pure sine: the largest sample-to-sample step of the output may not exceed that of an ideal
    // sine at the shifted frequency by more than 10% (a misaligned splice produces a step jump).
    const double sr = 48000;
    for (double semis : { -2.0, -0.5, 0.5, 2.0 })
        for (double f : { 100.0, 233.0, 600.0 })
        {
            const auto ratio = float (std::exp2 (semis / 12.0));
            auto out = shift (tone (sr, f, 0.5, false), sr, ratio, sr / f);
            double maxStep = 0;
            for (size_t n = 2000; n < out.size(); ++n) maxStep = std::max (maxStep, double (std::abs (out[n] - out[n - 1])));
            const double ideal = 2 * kPi * f * ratio / sr * 0.5;
            if (maxStep > ideal * 1.10) std::printf ("  f=%g semis=%g step %.5f vs ideal %.5f\n", f, semis, maxStep, ideal);
            CHECK (maxStep <= ideal * 1.10);
        }
}

TEST (shifter_robust_inputs)
{
    const double sr = 48000;
    std::mt19937 rng (3);
    std::normal_distribution<float> g (0.0f, 0.3f);
    std::vector<float> noise (24000);
    for (auto& v : noise) v = g (rng);
    for (double period : { 0.0, -5.0, 1e9, 37.3 }) // unvoiced, invalid, huge, arbitrary
    {
        auto out = shift (noise, sr, 1.3f, period, 17);
        float peak = 0;
        for (float v : out) { CHECK (std::isfinite (v)); peak = std::max (peak, std::abs (v)); }
        CHECK (peak < 2.0f);
    }
    auto out = shift (tone (sr, 220, 0.2, false), sr, 100.0f, sr / 220, 5); // ratio clamped, not UB
    for (float v : out) CHECK (std::isfinite (v));
}

TEST (fixed_delay)
{
    FixedDelay fd; fd.prepare (1, 5);
    std::vector<float> x (20);
    for (size_t i = 0; i < x.size(); ++i) x[i] = float (i + 1);
    float* p = x.data();
    fd.process (&p, 1, 7); p += 7; fd.process (&p, 1, 13);
    for (size_t i = 0; i < x.size(); ++i) CHECK (x[i] == (i < 5 ? 0.0f : float (i - 4)));
}

TEST (fixed_delay_change_is_smooth)
{
    FixedDelay fd; fd.prepare (1, 300, 240);
    std::vector<float> x (4800);
    for (size_t i = 0; i < x.size(); ++i) x[i] = float (0.5 * std::sin (2 * kPi * 220 * double (i) / 48000));
    float* p = x.data(); fd.process (&p, 1, 2400);
    fd.setDelay (120);
    p = x.data() + 2400; fd.process (&p, 1, 2400);
    double maxStep = 0;
    for (size_t i = 400; i < x.size(); ++i) maxStep = std::max (maxStep, double (std::abs (x[i] - x[i - 1])));
    CHECK (maxStep < 0.5 * 2 * kPi * 220 / 48000 * 1.05);
}

// ---------- correction engine ----------
static PitchEstimate est (double hz) { return { float (hz), 0.95f, true }; }

TEST (correction_amount_and_speed_zero)
{
    CorrectionEngine e; e.prepare (48000);
    CorrectionEngine::Params p; p.speedMs = 0;
    const double sharp = midiToFreq (69.30); // A4 + 30 c -> target A4
    for (double amount : { 0.0, 0.5, 1.0 })
    {
        p.amount = amount; e.setParams (p); e.reset();
        e.onEstimate (est (sharp));
        const double cents = 1200.0 * std::log2 (double (e.nextRatio()));
        CHECK_NEAR (cents, -30.0 * amount, 1e-3);
    }
}

TEST (correction_speed_time_constant)
{
    const double sr = 48000;
    for (double ms : { 5.0, 50.0, 200.0 })
    {
        CorrectionEngine e; e.prepare (sr);
        CorrectionEngine::Params p; p.speedMs = ms; e.setParams (p);
        e.onEstimate (est (midiToFreq (69.40)));
        float r = 1;
        for (int i = 0; i < int (ms / 1000 * sr); ++i) r = e.nextRatio();
        // After one time constant a one-pole reaches 1 - 1/e = 63.2% of the step.
        CHECK_NEAR (1200.0 * std::log2 (double (r)) / -40.0, 1.0 - std::exp (-1.0), 0.005);
    }
}

TEST (correction_negative_speed_and_edges)
{
    CorrectionEngine e; e.prepare (48000);
    CorrectionEngine::Params p;
    for (double s : { -3.0, -1.5, -0.01, -100.0, std::numeric_limits<double>::quiet_NaN() })
    {
        p.speedMs = s; e.setParams (p); e.reset();
        e.onEstimate (est (midiToFreq (60.2)));
        const float r = e.nextRatio();
        CHECK (std::isfinite (r));
        if (s >= -3.0 && s < 0) CHECK_NEAR (1200.0 * std::log2 (double (r)), -20.0, 1e-3); // instant
    }
    // -3 ms: zero hysteresis -> switches as soon as the midpoint is crossed.
    p.speedMs = -3; e.setParams (p); e.reset();
    e.onEstimate (est (midiToFreq (60.45))); CHECK (e.targetNote() == 60);
    e.onEstimate (est (midiToFreq (60.52))); CHECK (e.targetNote() == 61);
    // Empty mask and unvoiced input release to ratio 1.
    p.speedMs = 0; p.mask = 0; e.setParams (p); e.reset();
    e.onEstimate (est (midiToFreq (60.3)));
    CHECK (e.nextRatio() == 1.0f);
    p.mask = kAllNotes; e.setParams (p);
    e.onEstimate (est (midiToFreq (60.3))); e.nextRatio();
    e.onEstimate ({ 0, 0, false });
    float r = 0; for (int i = 0; i < 48000; ++i) r = e.nextRatio();
    CHECK_NEAR (r, 1.0, 1e-4);
}


// ---------- note stabilizer ----------
// Feeds `seconds` of a constant pitch as 2 ms estimates; returns the last target.
static int feed (CorrectionEngine& e, double midi, double seconds, float* lastRatio = nullptr)
{
    const int hops = int (std::lround (seconds / 0.002));
    for (int i = 0; i < hops; ++i)
    {
        e.onEstimate (est (midiToFreq (midi)));
        for (int k = 0; k < 96; ++k) { const float r = e.nextRatio(); if (lastRatio) *lastRatio = r; } // 2 ms of samples
    }
    return e.targetNote();
}

TEST (stabilizer_ignores_short_excursion)
{
    for (double ms : { 40.0, 80.0, 200.0 })
    {
        CorrectionEngine e; e.prepare (48000, 0.002);
        CorrectionEngine::Params p; p.speedMs = 0; p.stabilizerMs = ms; e.setParams (p);
        CHECK (feed (e, 60.1, 0.2) == 60);                // onset: confirmed immediately
        // Excursion to D (62) shorter than the threshold: no retarget, correction released (pitch passes).
        float r = 0;
        CHECK (feed (e, 62.2, ms / 1000.0 * 0.5, &r) == TargetSelector::kNoTarget);
        CHECK (e.pendingNote() == 62);
        CHECK (feed (e, 60.1, 0.05) == 60);               // back on C: old target, no D ever confirmed
        CHECK (e.pendingNote() == TargetSelector::kNoTarget);
    }
}

TEST (stabilizer_confirms_after_threshold)
{
    for (double ms : { 40.0, 80.0, 200.0 })
    {
        CorrectionEngine e; e.prepare (48000, 0.002);
        CorrectionEngine::Params p; p.speedMs = 0; p.stabilizerMs = ms; e.setParams (p);
        feed (e, 60.1, 0.2);
        // Count hops until 62 is confirmed: must be the threshold (+- one hop).
        int hops = 0;
        while (feed (e, 62.2, 0.002) != 62 && hops < 1000) ++hops;
        CHECK_NEAR ((hops + 1) * 2.0, ms, 2.0);
        float r = 0; feed (e, 62.2, 0.002, &r);
        CHECK_NEAR (1200.0 * std::log2 (double (r)), -20.0, 1e-3); // fully corrected once confirmed
    }
    // Off: retargets on the first estimate.
    CorrectionEngine e; e.prepare (48000, 0.002);
    CorrectionEngine::Params p; p.speedMs = 0; e.setParams (p);
    feed (e, 60.1, 0.1);
    CHECK (feed (e, 62.2, 0.002) == 62);
}

TEST (stabilizer_onset_after_silence_is_immediate)
{
    CorrectionEngine e; e.prepare (48000, 0.002);
    CorrectionEngine::Params p; p.speedMs = 0; p.stabilizerMs = 200; e.setParams (p);
    feed (e, 60.1, 0.2);
    for (int i = 0; i < 40; ++i) e.onEstimate ({ 0, 0, false }); // 80 ms gap > 60 ms onset gap
    CHECK (feed (e, 64.2, 0.002) == 64);
    // A short unvoiced gap (consonant) inside a phrase keeps the stabilizer engaged.
    for (int i = 0; i < 10; ++i) e.onEstimate ({ 0, 0, false }); // 20 ms
    CHECK (feed (e, 67.1, 0.002) == TargetSelector::kNoTarget);
    CHECK (e.pendingNote() == 67);
}

// ---------- sustain ----------
// Time for the correction to reach 63.2 % of a step applied after the note has been held `hold` s.
static double timeConstantAfterHold (double speedMs, double sustain, double hold)
{
    const double sr = 48000;
    CorrectionEngine e; e.prepare (sr, 0.002);
    CorrectionEngine::Params p; p.speedMs = speedMs; p.sustain = sustain; e.setParams (p);
    feed (e, 60.0, hold); // hold the note exactly in tune (no correction needed)
    // Step: same note, now 40 c sharp. Count samples until 63.2 %.
    int n = 0; double cents = 0;
    for (int hop = 0; hop < 5000 && cents > -40.0 * (1.0 - std::exp (-1.0)); ++hop)
    {
        e.onEstimate (est (midiToFreq (60.4)));
        for (int k = 0; k < 96 && cents > -40.0 * (1.0 - std::exp (-1.0)); ++k, ++n)
            cents = 1200.0 * std::log2 (double (e.nextRatio()));
    }
    return 1000.0 * n / sr;
}

TEST (sustain_scales_speed_with_note_age)
{
    const double base = timeConstantAfterHold (80, 0.0, 1.0);
    const double fast = timeConstantAfterHold (80, 1.0, 1.0);
    const double slow = timeConstantAfterHold (80, -1.0, 1.0);
    std::printf ("  tau after 1 s hold: sustain 0 %.1f ms, +100 %.1f ms, -100 %.1f ms\n", base, fast, slow);
    CHECK_NEAR (base, 80.0, 3.0);
    CHECK_NEAR (fast, 10.0, 2.5);   // 80 / 8
    CHECK_NEAR (slow, 640.0, 25.0); // 80 * 8
    // Young note (just started): sustain has barely any effect yet.
    CHECK_NEAR (timeConstantAfterHold (80, 1.0, 0.0), 80.0, 25.0);
    // Speed 0 stays instant whatever the sustain.
    CorrectionEngine e; e.prepare (48000, 0.002);
    CorrectionEngine::Params p; p.speedMs = 0; p.sustain = -1; e.setParams (p);
    feed (e, 60.0, 1.0);
    e.onEstimate (est (midiToFreq (60.3)));
    CHECK_NEAR (1200.0 * std::log2 (double (e.nextRatio())), -30.0, 1e-3);
    // Invalid values are clamped, never NaN.
    p.speedMs = 50; p.sustain = std::numeric_limits<double>::quiet_NaN(); p.stabilizerMs = -5; e.setParams (p);
    feed (e, 61.3, 0.1);
    CHECK (std::isfinite (e.nextRatio()));
}

TEST (end_to_end_correction)
{
    // Detuned harmonic tone (A3 + 35 c) through detector -> engine -> shifter at speed 0: output ~A3.
    const double sr = 48000, f = midiToFreq (57.35);
    auto x = tone (sr, f, 1.0, true);
    YinDetector det; det.prepare (sr, {});
    CorrectionEngine eng; eng.prepare (sr);
    CorrectionEngine::Params p; p.speedMs = 0; eng.setParams (p);
    PitchShifter ps; ps.prepare (sr, 1, 70.0);
    std::vector<float> ratio (32);
    for (size_t n = 0; n < x.size(); n += 32)
    {
        float* blk = x.data() + n;
        det.process (blk, 32, [&] (int, const PitchEstimate& e) {
            eng.onEstimate (e);
            ps.setPeriod (e.voiced ? sr / e.frequency : 0); });
        for (auto& r : ratio) r = eng.nextRatio();
        ps.process (&blk, 1, 32, ratio.data());
    }
    const double errC = 1200.0 * std::log2 (medianPitch (x, sr, 40) / midiToFreq (57.0));
    std::printf ("  residual %.2f c (input was +35 c)\n", errC);
    CHECK (std::abs (errC) < 3.0);
}


// ---------- HQ mode ----------
// Runs a stereo-identical signal through a chain; returns the left output.
static std::vector<float> runChain (const std::vector<float>& in, TunerChain::Settings s, double sr = 48000, int block = 64)
{
    TunerChain chain; chain.prepare (sr, 70.0);
    chain.setSettings (s); chain.reset();
    std::vector<float> L = in, R = in;
    for (size_t n = 0; n < L.size(); n += size_t (block))
    {
        const int len = int (std::min (size_t (block), L.size() - n));
        float* io[2] { L.data() + n, R.data() + n };
        chain.setSettings (s);
        chain.process (io, 2, len);
    }
    return L;
}

TEST (hq_latency_and_alignment)
{
    const double sr = 48000;
    for (double minF : { 70.0, 150.0 })
    {
        TunerChain::Settings s; s.minFreq = minF; s.correction.amount = 0; // ratio 1: wet = pure delay
        TunerChain a; a.prepare (sr, 70.0);
        s.hq = true; a.setSettings (s); a.reset();
        CHECK (a.latencySamples() == TunerChain::latencyFor (sr, minF, true));
        CHECK (TunerChain::latencyFor (sr, minF, true) - TunerChain::latencyFor (sr, minF, false) == 96);
        // With HQ on, wet output must equal the input delayed by exactly the reported latency.
        auto x = tone (sr, 220.0, 0.3, true);
        auto y = runChain (x, s);
        const size_t L = size_t (a.latencySamples());
        double maxErr = 0;
        for (size_t n = L + 4800; n < x.size(); ++n) maxErr = std::max (maxErr, double (std::abs (y[n] - x[n - L])));
        CHECK (maxErr < 1e-5);
    }
}

TEST (hq_tracks_glides_better)
{
    // Off-key glide (vocal-like scoop between notes): HQ must leave less residual error at Speed 0.
    const double sr = 48000;
    std::vector<float> x (size_t (sr * 1.5));
    double ph = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = double (n) / sr;
        const double midi = 57.3 + 2.0 * std::sin (2 * kPi * 0.8 * t); // slow 4-semitone swing
        ph += 2 * kPi * midiToFreq (midi) / sr;
        double v = 0;
        for (int h = 1; h <= 8; ++h) v += std::sin (h * ph) / h;
        x[n] = float (0.25 * v);
    }
    auto residual = [&] (bool hq) {
        TunerChain::Settings s; s.correction.speedMs = 0; s.minFreq = 150; s.hq = hq;
        auto y = runChain (x, s);
        YinDetector d; d.prepare (sr, {});
        std::vector<double> err;
        d.process (y.data() + int (sr * 0.2), int (y.size()) - int (sr * 0.2), [&] (int, const PitchEstimate& e) {
            if (e.voiced) { const double m = freqToMidi (e.frequency); err.push_back (std::abs (100.0 * (m - std::round (m)))); } });
        std::sort (err.begin(), err.end());
        return err[err.size() * 9 / 10];
    };
    const double off = residual (false), on = residual (true);
    std::printf ("  glide p90 residual: HQ off %.1f c, HQ on %.1f c\n", off, on);
    CHECK (on < off);
}

// ---------- doubler ----------
TEST (doubler_off_is_bit_exact)
{
    auto x = tone (48000, 233.0, 0.4, true);
    TunerChain::Settings s; s.correction.speedMs = 10;
    auto a = runChain (x, s);
    s.doubler.width = 1.0f; s.doubler.blend = 1.0f; // parameters set but disabled
    auto b = runChain (x, s);
    CHECK (a == b);
}

TEST (doubler_voices_detuned_late_and_wide)
{
    const double sr = 48000;
    Doubler d; d.prepare (sr);
    Doubler::Params p; p.enabled = true; p.width = 1.0f; p.blend = 1.0f; d.setParams (p); d.reset();
    d.setPeriod (sr / 220.0);
    // Feed a click train + tone; isolate voices by subtracting the scaled main (0.7 x input).
    auto x = tone (sr, 220.0, 1.2, false);
    std::vector<float> L = x, R = x;
    for (size_t n = 0; n < x.size(); n += 64)
    {
        float* io[2] { L.data() + n, R.data() + n };
        d.process (io, 2, int (std::min<size_t> (64, x.size() - n)));
    }
    std::vector<float> vl (x.size()), vr (x.size());
    for (size_t n = 0; n < x.size(); ++n) { vl[n] = L[n] - 0.7f * x[n]; vr[n] = R[n] - 0.7f * x[n]; }
    // With width 1, left holds voice 1 (-8 c) and right voice 2 (+6 c), each +-3 c wobble.
    const double cl = 1200.0 * std::log2 (medianPitch (vl, sr, 100) / 220.0);
    const double cr = 1200.0 * std::log2 (medianPitch (vr, sr, 100) / 220.0);
    std::printf ("  voice detune: L %.1f c, R %.1f c\n", cl, cr);
    CHECK_NEAR (cl, -8.0, 3.5);
    CHECK_NEAR (cr, 6.0, 3.5);
    // L and R must be decorrelated (not the same waveform panned).
    double sxy = 0, sxx = 0, syy = 0;
    for (size_t n = size_t (sr * 0.3); n < x.size(); ++n) { sxy += double (L[n]) * R[n]; sxx += double (L[n]) * L[n]; syy += double (R[n]) * R[n]; }
    const double corr = sxy / std::sqrt (sxx * syy);
    std::printf ("  L/R correlation at width 100 %%: %.3f\n", corr);
    CHECK (corr < 0.95);
}

TEST (doubler_gain_stable_and_safe)
{
    const double sr = 48000;
    auto x = tone (sr, 196.0, 0.8, true);
    auto rmsDb = [&] (float blend, bool en) {
        TunerChain::Settings s; s.correction.amount = 0; s.doubler.enabled = en; s.doubler.blend = blend; s.doubler.width = 0.7f;
        auto y = runChain (x, s);
        double e = 0; size_t cnt = 0;
        for (size_t n = size_t (sr * 0.3); n < y.size(); ++n, ++cnt) e += double (y[n]) * y[n];
        return 10.0 * std::log10 (e / double (cnt));
    };
    const double ref = rmsDb (0.0f, false);
    for (float b : { 0.25f, 0.5f, 1.0f })
    {
        const double dB = rmsDb (b, true) - ref;
        std::printf ("  blend %.0f %%: %+.2f dB\n", b * 100, dB);
        CHECK (std::abs (dB) < 2.0);
    }
    // Non-finite parameters and huge input stay finite.
    Doubler d; d.prepare (sr);
    Doubler::Params p; p.enabled = true; p.width = std::numeric_limits<float>::quiet_NaN(); p.blend = 7.0f; d.setParams (p);
    d.setPeriod (-1);
    std::vector<float> big (64, 1e6f);
    float* io[1] { big.data() };
    d.process (io, 1, 64);
    for (float v : big) CHECK (std::isfinite (v));
}

// ---------- full chain ----------
TEST (chain_rt_no_alloc_and_switching)
{
    const double sr = 48000;
    TunerChain chain; chain.prepare (sr, 70.0);
    std::vector<float> L (sr), R (sr);
    for (size_t n = 0; n < L.size(); ++n) { L[n] = harmonicTone (int (n), sr, midiToFreq (57.4), false); R[n] = 0.8f * L[n]; }
    TunerChain::Settings s; s.correction.speedMs = 0;
    const long before = gAllocs.load();
    double maxStep = 0;
    float prevL = 0;
    int b = 0;
    for (size_t n = 0; n + 64 <= L.size(); n += 64, ++b)
    {
        s.bypass = (b / 40) % 2 == 1;                  // toggle bypass every ~53 ms
        s.minFreq = (b / 100) % 2 ? 150.0 : 100.0;     // switch range every ~133 ms
        if (b == 300) s.correction.speedMs = -3;
        if (b == 500) { s.correction.stabilizerMs = 80; s.correction.sustain = 0.7; s.correction.speedMs = 30; }
        if (b == 550) { s.doubler.enabled = true; s.doubler.blend = 0.6f; s.hq = true; }
        if (b == 650) s.hq = false;
        chain.setSettings (s);
        float* io[2] { L.data() + n, R.data() + n };
        chain.process (io, 2, 64);
        for (size_t i = n; i < n + 64; ++i)
        {
            CHECK (std::isfinite (L[i]));
            const double st = b > 8 ? std::abs (L[i] - prevL) : 0.0; // skip the abrupt test-signal onset
            maxStep = std::max (maxStep, st); prevL = L[i];
        }
    }
    CHECK (gAllocs.load() == before);
    // Input's own max step is ~0.09 (and the signal's onset is skipped); a click is several times larger.
    std::printf ("  max sample step with bypass/range switching: %.3f\n", maxStep);
    CHECK (maxStep < 0.15);
}

int main() { return th::runAll(); }
