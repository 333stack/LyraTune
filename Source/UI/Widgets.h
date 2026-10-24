#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace icons
{
juce::Path undo();
juce::Path redo();
juce::Path reset();
juce::Path power();
juce::Path eye();
juce::Path notes();
juce::Path help();
juce::Path menu();
juce::Path copyArrow();
juce::Path bolt();
}

// Flat icon button; toggleable buttons light up in the accent colour when on.
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, juce::Path icon, juce::String caption = {});
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::Colour onColour = theme::violet;
    float glowStrength = 0.55f; // selected glow of captioned toggles

private:
    juce::Path icon;
    juce::String caption;
};

// Text pill button (A/B slots).
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& text) : juce::Button (text) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Row of pills bound to a choice parameter. Each pill maps to one choice index.
class SegmentedControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedControl (juce::RangedAudioParameter&, juce::StringArray labels, juce::UndoManager*);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

private:
    int indexAt (float x) const { return juce::jlimit (0, labels.size() - 1, int (x / (float (getWidth()) / float (labels.size())))); }
    juce::StringArray labels;
    juce::ParameterAttachment attach;
    int selected = 0, hover = -1;
};

// Compact horizontal value control: "440.0 Hz" over a thin track. Drag, Shift = fine, double-click = default.
class ValueStrip : public juce::Slider
{
public:
    ValueStrip();
    void paint (juce::Graphics&) override;
    bool fillFromLeft = false; // default: fill from the centre (bipolar values such as pitch reference)
};
