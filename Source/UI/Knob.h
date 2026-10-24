#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Custom rotary control. Drag vertically/horizontally; Shift = fine; double-click = default;
// right-click = type a value. Attach with AudioProcessorValueTreeState::SliderAttachment.
class Knob : public juce::Slider, private juce::Timer
{
public:
    Knob (juce::String label, juce::String unit, std::function<juce::String (double)> format, bool reversed = false);

    // Bipolar: lit ticks run from the centre (12 o'clock) to the value, e.g. Sustain -100..+100.
    void setBipolar (bool b) { bipolar = b; repaint(); }

    // Marked detent at `value`: a notch on the ring, and drags within +-`snapRange` stick to it.
    // Values below it "overload" the knob: hot flickering ring, pulsing glow, tremble, and a
    // shockwave when crossing. Animation runs only while overloaded.
    void setOverloadDetent (double value, double snapRange);

    // Extra space around the dial for the glow; the dial radius shrinks by this many pixels.
    void setGlowMargin (float px) { glowMargin = px; repaint(); }

    // Reversed: the minimum sits at the clockwise end (Speed: -3 ms = full arc, clockwise = harder).
    double proportionOfLengthToValue (double p) override { return Slider::proportionOfLengthToValue (reversed ? 1.0 - p : p); }
    double valueToProportionOfLength (double v) override { const double p = Slider::valueToProportionOfLength (v); return reversed ? 1.0 - p : p; }
    double snapValue (double attempted, DragMode mode) override;

    void paint (juce::Graphics&) override;
    void valueChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void timerCallback() override;
    void showValueEditor();
    float overloadAmount() const; // 0 = at/above the detent, 1 = at the minimum

    juce::String label, unit;
    bool reversed = false, bipolar = false;
    std::function<juce::String (double)> format;
    std::unique_ptr<juce::TextEditor> editor;

    bool hasDetent = false;
    double detent = 0, detentSnap = 0;
    float glowMargin = 0, phase = 0, burst = 0;
    bool wasOverloaded = false;
};
