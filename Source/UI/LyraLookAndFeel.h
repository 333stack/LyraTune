#pragma once
#include "Theme.h"

// Styles the stock JUCE pieces we still use: popup menus, tooltips, text editors.
class LyraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LyraLookAndFeel();

    juce::Font getPopupMenuFont() override { return theme::font (14.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
};
