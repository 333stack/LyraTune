// Offline render through the plugin's exact TunerChain (same code path as the VST3).
// stdin/stdout: raw float32 interleaved stereo. Example:
//   ffmpeg -v error -i in.wav -ac 2 -f f32le - | build/tests/render_tuner 48000 0 100 0 1 2 \
//     | ffmpeg -v error -f f32le -ar 48000 -ac 2 -i - out.wav
// args: sampleRate speedMs amount% root(0=C..11) scale(0 chromatic,1 major,2 minor) range(0 low,1 mid,2 high)
//       [refHz] [stabilizerMs] [sustain -100..100] [hq 0/1] [doubler blend % (0 = off)]
// stderr reports target-note changes per minute and stabilizer holds (from the chain's telemetry).
// Output is shifted earlier by the reported latency so it lines up with the input file.
#include "TunerChain.h"
#include "Pitch/Scale.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main (int argc, char** argv)
{
    if (argc < 7) { std::fprintf (stderr, "usage: render_tuner sr speedMs amount root scale range [refHz]\n"); return 2; }
    const double sr = std::atof (argv[1]);
    static constexpr double rangeHz[] { 70.0, 100.0, 150.0 };
    lyra::TunerChain chain;
    chain.prepare (sr, rangeHz[0]);
    lyra::TunerChain::Settings s;
    s.correction.speedMs = std::atof (argv[2]);
    s.correction.amount = std::atof (argv[3]) / 100.0;
    s.correction.mask = lyra::scaleMask (lyra::ScaleType (std::atoi (argv[5]) % 3), std::atoi (argv[4]));
    s.minFreq = rangeHz[std::atoi (argv[6]) % 3];
    if (argc > 7) s.correction.referenceHz = std::atof (argv[7]);
    if (argc > 8) s.correction.stabilizerMs = std::atof (argv[8]);
    if (argc > 9) s.correction.sustain = std::atof (argv[9]) / 100.0;
    if (argc > 10) s.hq = std::atoi (argv[10]) != 0;
    if (argc > 11) { s.doubler.blend = float (std::atof (argv[11]) / 100.0); s.doubler.enabled = s.doubler.blend > 0; }
    static lyra::TelemetryQueue telemetry; // static: large, keep off the stack
    chain.setTelemetryQueue (&telemetry);
    long switches = 0, holds = 0, voicedHops = 0;
    int lastTarget = -1, lastPending = -1;
    auto drain = [&] {
        lyra::Telemetry t;
        while (telemetry.pop (t))
        {
            if (t.target >= 0) { ++voicedHops; if (lastTarget >= 0 && t.target != lastTarget) ++switches; lastTarget = t.target; }
            if (t.pending >= 0 && t.pending != lastPending) ++holds;
            lastPending = t.pending;
        } };
    chain.setSettings (s);
    chain.reset(); // apply the range without a dry-delay crossfade at t=0
    const int latency = chain.latencySamples();
    std::fprintf (stderr, "latency %d samples (%.2f ms)\n", latency, 1000.0 * latency / sr);

    constexpr int B = 128; // a typical host buffer
    std::vector<float> inter (2 * B), l (B), r (B);
    long skip = latency;
    auto emit = [&] (int n) {
        for (int i = 0; i < n; ++i) { inter[size_t (2 * i)] = l[size_t (i)]; inter[size_t (2 * i + 1)] = r[size_t (i)]; }
        const int off = int (std::min<long> (skip, n));
        skip -= off;
        std::fwrite (inter.data() + 2 * off, sizeof (float), size_t (2 * (n - off)), stdout); };
    size_t got;
    while ((got = std::fread (inter.data(), sizeof (float), inter.size(), stdin)) > 0)
    {
        const int n = int (got / 2);
        for (int i = 0; i < n; ++i) { l[size_t (i)] = inter[size_t (2 * i)]; r[size_t (i)] = inter[size_t (2 * i + 1)]; }
        float* io[2] { l.data(), r.data() };
        chain.process (io, 2, n);
        drain();
        emit (n);
    }
    std::fprintf (stderr, "target changes %.1f/min of corrected audio, stabilizer holds %ld\n",
                  voicedHops ? switches * 60.0 / (voicedHops * 0.002) : 0.0, holds);
    // Flush the tail so output length matches input.
    std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f);
    for (int left = latency; left > 0; left -= B)
    {
        float* io[2] { l.data(), r.data() };
        const int n = std::min (B, left);
        chain.process (io, 2, n);
        emit (n);
    }
}
