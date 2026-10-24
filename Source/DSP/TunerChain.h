#pragma once
#include "Common/FixedDelay.h"
#include "Common/SpscQueue.h"
#include "Pitch/CorrectionEngine.h"
#include "Pitch/PitchShifter.h"
#include "Pitch/YinDetector.h"

namespace lyra
{
// The complete real-time tuner signal path, independent of any plugin framework so the plugin and
// the offline render/test tools run identical code:
//   mono sum -> YinDetector -> CorrectionEngine -> PitchShifter (stereo-linked)
//   dry path delayed by the same latency; bypass = 10 ms linear crossfade between the two.
//
// One display sample per detector hop, sent to the UI through a lock-free queue.
struct Telemetry
{
    float inputMidi = 0;  // fractional MIDI of the detected pitch; 0 = unvoiced
    float outputMidi = 0; // inputMidi + applied correction
    float confidence = 0;
    float rms = 0;
    int target = -1;      // target MIDI note, -1 = none
};
using TelemetryQueue = SpscQueue<Telemetry, 1024>; // ~2 s at the 2 ms hop

// Threading: prepare() allocates; everything else is RT-safe and audio-thread owned.
class TunerChain
{
public:
    static constexpr int kSubBlock = 32; // control-rate granularity (0.67 ms @48 kHz)
    static constexpr int kMaxChannels = 2;

    struct Settings
    {
        CorrectionEngine::Params correction;
        double minFreq = 70.0; // range floor: sets detector frame and shifter latency
        bool bypass = false;
    };

    void prepare (double sampleRate, double lowestMinFreq);
    void reset() noexcept;
    void setSettings (const Settings&) noexcept; // call once per block
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    int latencySamples() const noexcept { return shifter.latencySamples(); }
    static int latencyFor (double sampleRate, double minFreq) noexcept { return PitchShifter::latencyFor (sampleRate, minFreq); }

    // Optional; the queue must outlive the chain. Set before processing starts.
    void setTelemetryQueue (TelemetryQueue* q) noexcept { telemetry = q; }

    const CorrectionEngine& correction() const noexcept { return engine; }
    const YinDetector& detector() const noexcept { return yin; }

private:
    YinDetector yin;
    CorrectionEngine engine;
    PitchShifter shifter;
    FixedDelay dryDelay;
    TelemetryQueue* telemetry = nullptr;
    double sr = 48000, minFreq = 70;
    float wet = 1, wetTarget = 1, wetStep = 0.01f;
    float mono[kSubBlock] {}, ratio[kSubBlock] {}, dry[kMaxChannels][kSubBlock] {};
};
} // namespace lyra
