// Offline vocal analysis with the plugin's own YinDetector.
// Optional 3rd arg: CSV path for the per-hop track.
// Input: raw float32 mono at the given rate on stdin, e.g.
//   ffmpeg -v error -i vocal.wav -ac 1 -f f32le - | build/tests/analyze_vocal 48000 [minHz]
// Reports pitch range, tuning offset vs A440, a Krumhansl-Schmuckler key estimate, and octave-jump counts.
#include "Pitch/PitchMath.h"
#include "Pitch/YinDetector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace lyra;

int main (int argc, char** argv)
{
    const double sr = argc > 1 ? std::atof (argv[1]) : 48000.0;
    YinDetector::Config cfg;
    if (argc > 2) cfg.minFreq = std::atof (argv[2]);
    YinDetector det;
    det.prepare (sr, cfg);

    std::vector<float> buf (4096);
    std::vector<PitchEstimate> est;
    size_t got, total = 0;
    while ((got = std::fread (buf.data(), sizeof (float), buf.size(), stdin)) > 0)
    {
        det.process (buf.data(), int (got), [&] (int, const PitchEstimate& e) { est.push_back (e); });
        total += got;
    }

    std::vector<double> midi, cents;
    double pcWeight[12] {};
    int octaveJumps = 0, semitoneJumpsLarge = 0;
    double prev = 0;
    for (auto& e : est)
    {
        if (! e.voiced || e.confidence < 0.7f) { prev = 0; continue; }
        const double m = freqToMidi (e.frequency);
        midi.push_back (m);
        cents.push_back (100.0 * (m - std::round (m)));
        pcWeight[pitchClassOf (int (std::lround (m)))] += e.confidence;
        if (prev > 0)
        {
            const double jump = std::abs (m - prev);
            octaveJumps += jump > 10.5 && jump < 13.5; // adjacent hops 5 ms apart: ~12 st jump is almost surely an octave error
            semitoneJumpsLarge += jump > 4.0;
        }
        prev = m;
    }
    if (midi.empty()) { std::puts ("no voiced frames"); return 1; }
    if (argc > 3) // optional per-hop CSV: time, freq, confidence, voiced
        if (FILE* f = std::fopen (argv[3], "w"))
        {
            const double hopSec = det.hopSamples() / sr;
            for (size_t i = 0; i < est.size(); ++i)
                std::fprintf (f, "%.4f,%.2f,%.3f,%d\n", double (i) * hopSec, est[i].frequency, est[i].confidence, int (est[i].voiced));
            std::fclose (f);
        }

    auto pct = [] (std::vector<double> v, double p) { std::sort (v.begin(), v.end()); return v[size_t (p * double (v.size() - 1))]; };
    static const char* names[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto noteName = [&] (double m) { static char s[16]; std::snprintf (s, sizeof s, "%s%d", names[pitchClassOf (int (std::lround (m)))], int (std::lround (m)) / 12 - 1); return s; };

    std::printf ("duration %.1f s, estimates %zu, voiced(conf>=0.7) %.1f%%\n", double (total) / sr, est.size(), 100.0 * double (midi.size()) / double (est.size()));
    for (double p : { 0.01, 0.05, 0.5, 0.95, 0.99 })
        std::printf ("  p%02.0f: %6.1f Hz  %s\n", p * 100, midiToFreq (pct (midi, p)), noteName (pct (midi, p)));
    std::printf ("tuning: median offset from nearest semitone %.1f c (A440 grid)\n", pct (cents, 0.5));
    std::printf ("adjacent-hop jumps: ~octave %d, >4 st %d (of %zu voiced)\n", octaveJumps, semitoneJumpsLarge, midi.size());

    // Krumhansl-Kessler key profiles.
    const double maj[12] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    const double minr[12] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    auto corr = [&] (const double* prof, int root) {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += pcWeight[(i + root) % 12]; my += prof[i]; }
        mx /= 12; my /= 12;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i) { const double a = pcWeight[(i + root) % 12] - mx, b = prof[i] - my; sxy += a * b; sxx += a * a; syy += b * b; }
        return sxy / std::sqrt (sxx * syy);
    };
    struct K { double r; int root; bool minor; };
    std::vector<K> keys;
    for (int r = 0; r < 12; ++r) { keys.push_back ({ corr (maj, r), r, false }); keys.push_back ({ corr (minr, r), r, true }); }
    std::sort (keys.begin(), keys.end(), [] (auto& a, auto& b) { return a.r > b.r; });
    std::printf ("pitch-class weights:");
    double sum = 0; for (double w : pcWeight) sum += w;
    for (int i = 0; i < 12; ++i) std::printf (" %s=%.1f%%", names[i], 100 * pcWeight[i] / sum);
    std::printf ("\nkey candidates:");
    for (int i = 0; i < 4; ++i) std::printf ("  %s %s (r=%.3f)", names[keys[size_t (i)].root], keys[size_t (i)].minor ? "minor" : "major", keys[size_t (i)].r);
    std::printf ("\n");
}
