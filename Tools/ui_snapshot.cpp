// Renders the real editor to PNG for visual review (runs under Wine; no host needed).
// Feeds the processor a synthetic, slightly sharp harmonic tone so the visualisers show real DSP data.
// usage: ui_snapshot.exe <out-dir> [scale] [major|bypass]
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    const double sr = 48000;
    proc->setPlayConfigDetails (2, 2, sr, 128);
    proc->prepareToPlay (sr, 128);

    const juce::String mode = argc > 3 ? juce::String (argv[3]) : juce::String();
    auto set = [&] (const char* id, float v) {
        for (auto* p : proc->getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp && rp->getParameterID() == id)
                rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    };
    if (mode == "major") { set ("root", 4); set ("scale", 1); set ("range", 2); set ("speed", -3); set ("amount", 72); set ("stabilizer", 2); set ("sustain", 35); }
    if (mode == "bypass") set ("bypass", 1);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditorAndMakeActive());
    ed->setSize (juce::roundToInt (900 * scale), juce::roundToInt (600 * scale));

    // E4 + 28 cents with vibrato-free harmonics, ~1 s, interleaved with message-loop time so timers run.
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;
    const double f = 440.0 * std::pow (2.0, (64.28 - 69.0) / 12.0);
    long n = 0;
    for (int block = 0; block < 375; ++block)
    {
        for (int i = 0; i < 128; ++i, ++n)
        {
            double s = 0;
            for (int h = 1; h <= 10; ++h) s += std::sin (2 * juce::MathConstants<double>::pi * f * h * double (n) / sr + h) / h;
            buf.setSample (0, i, float (0.2 * s));
            buf.setSample (1, i, float (0.2 * s));
        }
        proc->processBlock (buf, midi);
        if (block % 4 == 0)
        {
            juce::Thread::sleep (10); // real time must pass for timers to become due
            juce::Timer::callPendingTimersSynchronously();
        }
    }
    for (int i = 0; i < 3; ++i) { juce::Thread::sleep (20); juce::Timer::callPendingTimersSynchronously(); }

    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    const auto file = outDir.getChildFile ("ui_" + juce::String (juce::roundToInt (scale * 100)) + mode + ".png");
    file.deleteFile();
    juce::FileOutputStream os (file);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
