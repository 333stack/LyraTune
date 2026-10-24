#pragma once
#include "DisplayState.h"
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Central visualiser. Shows real tuner data:
//  - core ring colours rotate with the output pitch class, brightness follows input level;
//  - an arc on the middle ring spans detected pitch -> corrected pitch (length = correction);
//  - centre text: target note and the input's deviation from it.
// The small handle is an XY pad: right = faster Speed, up = more Amount.
class Orb : public juce::Component, public juce::SettableTooltipClient
{
public:
    Orb (juce::RangedAudioParameter& speed, juce::RangedAudioParameter& amount, juce::UndoManager*);

    void setDisplay (const DisplayState& s, bool bypassed);
    void setDoubler (bool on, float width, float blend) { dblOn = on; dblWidth = width; dblBlend = blend; }
    void paint (juce::Graphics&) override;
    void resized() override { background = {}; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverHandle = false; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Point<float> handlePos() const;
    juce::Rectangle<float> padArea() const;
    void renderBackground();

    juce::RangedAudioParameter& speedParam;
    juce::RangedAudioParameter& amountParam;
    juce::ParameterAttachment speedAttach, amountAttach;
    float speedNorm = 0, amountNorm = 1;
    bool dragging = false, hoverHandle = false, bypassed = false, dblOn = false;
    float dblWidth = 0.7f, dblBlend = 0.5f, dblFade = 0.0f;
    DisplayState state;
    juce::Image background;
};
