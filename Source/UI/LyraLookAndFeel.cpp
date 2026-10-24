#include "LyraLookAndFeel.h"

LyraLookAndFeel::LyraLookAndFeel()
{
    using namespace theme;
    setColour (juce::PopupMenu::backgroundColourId, bg2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::TextEditor::backgroundColourId, bg0);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, violet.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, violet);
    setColour (juce::TextEditor::focusedOutlineColourId, violet);
    setColour (juce::CaretComponent::caretColourId, lavender);
    setColour (juce::TooltipWindow::backgroundColourId, bg3);
    setColour (juce::TooltipWindow::textColourId, text);
}

void LyraLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::bg2);
    g.setColour (theme::line);
    g.drawRect (0, 0, w, h);
}

juce::Rectangle<int> LyraLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (tip, theme::font (13.0f), theme::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 260.0f);
    const int w = int (tl.getWidth()) + 20, h = int (tl.getHeight()) + 14;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 16,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}

void LyraLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int w, int h)
{
    const auto r = juce::Rectangle<float> (0, 0, float (w), float (h));
    g.setColour (theme::bg3);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    juce::AttributedString s;
    s.append (tip, theme::font (13.0f), theme::text);
    s.setJustification (juce::Justification::centredLeft);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 260.0f);
    tl.draw (g, r.reduced (10.0f, 7.0f));
}
