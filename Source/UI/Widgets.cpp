#include "Widgets.h"

namespace icons
{
static juce::Path undoShape()
{
    // Arc over the top from 9 o'clock to 4 o'clock, arrowhead on the left end pointing down.
    juce::Path p;
    p.addCentredArc (12, 13, 7.5f, 6.5f, 0, -juce::MathConstants<float>::halfPi, 2.3f, true);
    const juce::Point<float> tip (4.5f, 13.0f);
    p.startNewSubPath (tip.translated (-3.2f, -3.0f));
    p.lineTo (tip.translated (0.0f, 0.8f));
    p.lineTo (tip.translated (3.2f, -3.0f));
    return p;
}
juce::Path undo() { return undoShape(); }
juce::Path redo() { auto p = undoShape(); p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, 12.0f, 12.0f)); return p; }
juce::Path reset()
{
    juce::Path p;
    p.addCentredArc (12, 12, 7, 7, 0, 0.6f, 5.6f, true);
    p.startNewSubPath (13.5f, 1.5f); p.lineTo (16.5f, 5.0f); p.lineTo (12.5f, 7.0f);
    return p;
}
juce::Path power()
{
    juce::Path p;
    p.addCentredArc (12, 13, 7, 7, 0, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    p.startNewSubPath (12, 3); p.lineTo (12, 11);
    return p;
}
juce::Path eye()
{
    juce::Path p;
    p.startNewSubPath (3, 12); p.quadraticTo (12, 3, 21, 12); p.quadraticTo (12, 21, 3, 12); p.closeSubPath();
    p.addEllipse (9, 9, 6, 6);
    return p;
}
juce::Path notes()
{
    juce::Path p;
    for (int i = 0; i < 4; ++i) { p.startNewSubPath (4.0f + 5.0f * float (i), 18); p.lineTo (4.0f + 5.0f * float (i), 6.0f + float (i % 2) * 4.0f); }
    p.startNewSubPath (2, 20); p.lineTo (22, 20);
    return p;
}
juce::Path help()
{
    juce::Path p;
    p.addEllipse (3, 3, 18, 18);
    p.startNewSubPath (9.2f, 9.5f); p.cubicTo (9.2f, 6.0f, 15.0f, 6.0f, 15.0f, 9.5f); p.cubicTo (15.0f, 12.0f, 12.0f, 11.8f, 12.0f, 14.5f);
    p.addEllipse (11.2f, 16.6f, 1.6f, 1.6f);
    return p;
}
juce::Path menu()
{
    juce::Path p;
    for (int i = 0; i < 3; ++i) { p.startNewSubPath (5, 7.0f + 5.0f * float (i)); p.lineTo (19, 7.0f + 5.0f * float (i)); }
    return p;
}
juce::Path copyArrow()
{
    juce::Path p; // small chevron: "copy this slot to the other"
    p.startNewSubPath (9, 6); p.lineTo (15, 12); p.lineTo (9, 18);
    return p;
}
juce::Path bolt()
{
    juce::Path p;
    p.startNewSubPath (13, 2); p.lineTo (5, 13); p.lineTo (11, 13); p.lineTo (10, 22); p.lineTo (19, 10); p.lineTo (13, 10); p.closeSubPath();
    return p;
}
} // namespace icons

IconButton::IconButton (const juce::String& name, juce::Path i, juce::String c)
    : juce::Button (name), icon (std::move (i)), caption (std::move (c))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool hi, bool down)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat();
    if (hi || down)
    {
        g.setColour (down ? bg3.brighter (0.1f) : bg3);
        g.fillRoundedRectangle (r.reduced (1.0f), 5.0f);
    }
    const bool on = getToggleState();
    if (on && caption.isNotEmpty() && isEnabled()) // captioned setting toggles: quiet selected treatment
        drawSelectionGlow (g, r.reduced (2.0f, 3.0f), onColour, glowStrength);
    const auto col = ! isEnabled() ? textFaint : (on ? onColour : (hi ? theme::text : textDim));
    auto iconArea = caption.isEmpty() ? r : r.removeFromLeft (r.getHeight());
    const float s = juce::jmin (iconArea.getWidth(), iconArea.getHeight()) * 0.62f;
    g.setColour (col);
    g.strokePath (icon, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  icon.getTransformToScaleToFit (juce::Rectangle<float> (s, s).withCentre (iconArea.getCentre()), true));
    if (caption.isNotEmpty())
    {
        g.setColour (on ? theme::text : textDim);
        g.setFont (font (12.0f, true));
        g.drawFittedText (caption, r.toNearestInt().withTrimmedRight (2), juce::Justification::centredLeft, 2, 0.9f);
    }
}

void PillButton::paintButton (juce::Graphics& g, bool hi, bool)
{
    using namespace theme;
    const auto r = getLocalBounds().toFloat().reduced (1.0f, 4.0f);
    const bool on = getToggleState();
    if (on)
    {
        g.setColour (bg3);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        drawSelectionGlow (g, r);
    }
    else if (hi)
    {
        g.setColour (bg3);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (on ? lavender : (hi ? theme::text : textDim));
    g.setFont (font (14.0f, true));
    g.drawText (getButtonText(), r, juce::Justification::centred);
}

SegmentedControl::SegmentedControl (juce::RangedAudioParameter& p, juce::StringArray l, juce::UndoManager* um)
    : labels (std::move (l)),
      attach (p, [this] (float v) { selected = juce::roundToInt (v); repaint(); }, um)
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentedControl::paint (juce::Graphics& g)
{
    using namespace theme;
    // Inset track; the halo of the selected pill needs a little room, so the track is inset 2 px.
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (bg0.withAlpha (0.85f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); // inner shadow line: reads as recessed
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    const float w = r.getWidth() / float (labels.size());
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto cellR = juce::Rectangle<float> (r.getX() + w * float (i), r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == selected)
        {
            g.setGradientFill (juce::ColourGradient (bg3.brighter (0.15f), cellR.getX(), cellR.getY(), bg3, cellR.getX(), cellR.getBottom(), false));
            g.fillRoundedRectangle (cellR, cellR.getHeight() * 0.5f);
            drawSelectionGlow (g, cellR);
        }
        else if (i == hover)
        {
            g.setColour (bg3.withAlpha (0.5f));
            g.fillRoundedRectangle (cellR, cellR.getHeight() * 0.5f);
        }
        g.setColour (i == selected ? lavender : (i == hover ? text : textDim));
        g.setFont (font (13.0f, true));
        g.drawText (labels[i], cellR, juce::Justification::centred);
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position.x);
    if (i != selected)
        attach.setValueAsCompleteGesture (float (i));
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position.x);
    if (i != hover) { hover = i; repaint(); }
}

ValueStrip::ValueStrip()
{
    setSliderStyle (LinearHorizontal);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setMouseDragSensitivity (300);
    setVelocityModeParameters (0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    setSliderSnapsToMousePosition (false);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void ValueStrip::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat();
    const auto track = r.removeFromBottom (4.0f).reduced (2.0f, 0.0f);
    g.setColour (bg0);
    g.fillRoundedRectangle (track, 2.0f);
    const float t = float (valueToProportionOfLength (getValue()));
    const float mid = fillFromLeft ? track.getX() : track.getCentreX(), x = track.getX() + t * track.getWidth();
    const auto bar = juce::Rectangle<float> (juce::jmin (mid, x), track.getY(), std::abs (x - mid) + 1.5f, track.getHeight());
    g.setColour (violet.withAlpha (0.16f)); // soft glow under the value bar
    g.fillRoundedRectangle (bar.expanded (1.5f, 2.0f), 3.5f);
    g.setGradientFill (juce::ColourGradient (violet.interpolatedWith (lavender, 0.25f), bar.getX(), bar.getY(), violet, bar.getX(), bar.getBottom(), false));
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (isMouseOverOrDragging() ? text : text.withAlpha (0.85f));
    g.setFont (font (14.0f, true));
    g.drawText (getTextFromValue (getValue()), r, juce::Justification::centred);
}
