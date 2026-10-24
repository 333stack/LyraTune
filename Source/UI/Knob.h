#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Custom rotary control. Drag vertically/horizontally; Shift = fine; double-click = default;
// right-click = type a value. Attach with AudioProcessorValueTreeState::SliderAttachment.
class Knob : public juce::Slider
{
public:
    Knob (juce::String label, juce::String unit, std::function<juce::String (double)> format, bool reversed = false);

    // Reversed: the minimum sits at the clockwise end (Speed: -3 ms = full arc, clockwise = harder).
    double proportionOfLengthToValue (double p) override { return Slider::proportionOfLengthToValue (reversed ? 1.0 - p : p); }
    double valueToProportionOfLength (double v) override { const double p = Slider::valueToProportionOfLength (v); return reversed ? 1.0 - p : p; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void showValueEditor();

    juce::String label, unit;
    bool reversed = false;
    std::function<juce::String (double)> format;
    std::unique_ptr<juce::TextEditor> editor;
};
