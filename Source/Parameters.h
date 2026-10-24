#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs are part of saved projects/automation: never rename or reuse them.
namespace ParamID
{
inline constexpr auto bypass = "bypass";
inline constexpr auto speed = "speed";
inline constexpr auto amount = "amount";
inline constexpr auto root = "root";
inline constexpr auto scale = "scale";
inline constexpr auto referenceHz = "refHz";
inline constexpr auto range = "range";
inline juce::String note (int pitchClass) { return "note" + juce::String (pitchClass); } // note0 (C) .. note11 (B)
}

inline const juce::StringArray kNoteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
// Index order must match lyra::ScaleType. "Custom" uses the twelve noteN parameters.
// (Pre-release change: Custom appended in 0.1; choice normalisation changed, so v=2 for `scale`.)
inline const juce::StringArray kScaleNames { "Chromatic", "Major", "Minor", "Custom" };

// Range = lowest pitch the tuner tracks. It sets the shifter's latency, so it is deliberately NOT
// automatable: hosts re-align plugin delay compensation only when latency changes outside playback.
inline const juce::StringArray kRangeNames { "Low (70 Hz)", "Mid (100 Hz)", "High (150 Hz)" };
inline constexpr double kRangeMinHz[] { 70.0, 100.0, 150.0 };
inline double rangeMinHz (float index) { return kRangeMinHz[juce::jlimit (0, 2, int (index))]; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    constexpr int v = 1; // version hint for parameters introduced in this release

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::bypass, v }, "Bypass", false));

    // -3..0 ms = aggressive mode (see CorrectionEngine); skewed so the musically busy 0-60 ms range gets most travel.
    NormalisableRange<float> speedRange (-3.0f, 400.0f, 0.1f);
    speedRange.setSkewForCentre (40.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::speed, v }, "Speed", speedRange, 20.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float x, int) { return String (x, x < 10.0f ? 1 : 0) + " ms"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::amount, v }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float x, int) { return String (x, 0) + " %"; })));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::root, v }, "Root", kNoteNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::scale, 2 }, "Scale", kScaleNames, 0));
    for (int pc = 0; pc < 12; ++pc)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::note (pc), 2 },
                                                          "Note " + kNoteNames[pc], true));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::referenceHz, v }, "Reference",
        NormalisableRange<float> (420.0f, 460.0f, 0.1f), 440.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
            [] (float x, int) { return String (x, 1) + " Hz"; })));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::range, v }, "Range", kRangeNames, 0,
        AudioParameterChoiceAttributes().withAutomatable (false)));
    return layout;
}
