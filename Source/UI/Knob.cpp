#include "Knob.h"

static constexpr float kStartAngle = juce::MathConstants<float>::pi * -0.75f; // 7:30 o'clock
static constexpr float kEndAngle = juce::MathConstants<float>::pi * 0.75f;    // 4:30 o'clock
static constexpr int kTicks = 56;

Knob::Knob (juce::String l, juce::String u, std::function<juce::String (double)> f, bool rev)
    : label (std::move (l)), unit (std::move (u)), reversed (rev), format (std::move (f))
{
    setSliderStyle (RotaryHorizontalVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setRotaryParameters (kStartAngle + juce::MathConstants<float>::twoPi, kEndAngle + juce::MathConstants<float>::twoPi, true);
    setMouseDragSensitivity (220);
    // Holding Shift switches to velocity mode with low sensitivity: fine adjustment.
    setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    setWantsKeyboardFocus (true);
}

void Knob::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float t = float (valueToProportionOfLength (getValue()));
    const bool hot = isMouseOverOrDragging() || hasKeyboardFocus (false);

    // Outer well with soft shadow.
    juce::Path well;
    well.addEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), int (R * 0.12f), { 0, int (R * 0.04f) }).drawForPath (g, well);
    g.setGradientFill (juce::ColourGradient (bg2, c.x, c.y - R, bg0, c.x, c.y + R, false));
    g.fillPath (well);

    // Tick ring: lit ticks follow the accent gradient up to the value.
    const float r1 = R * 0.80f, r2 = R * 0.93f, tickW = juce::jmax (1.6f, R * 0.028f);
    const int lit = juce::roundToInt (t * (kTicks - 1));
    for (int i = 0; i < kTicks; ++i)
    {
        const float a = kStartAngle + (kEndAngle - kStartAngle) * float (i) / float (kTicks - 1);
        const juce::Point<float> dir (std::sin (a), -std::cos (a));
        juce::Colour col = i <= lit ? accentAt (float (i) / float (kTicks - 1)) : line.brighter (hot ? 0.25f : 0.0f);
        if (i == lit)
            col = juce::Colours::white;
        const float ext = i == lit ? R * 0.035f : 0.0f;
        g.setColour (col);
        g.drawLine ({ c + dir * (r1 - ext), c + dir * (r2 + ext) }, i == lit ? tickW * 1.6f : tickW);
    }

    // Raised inner disc.
    const float ri = R * 0.70f;
    g.setGradientFill (juce::ColourGradient (bg3.brighter (0.08f), c.x, c.y - ri, bg1, c.x, c.y + ri, false));
    g.fillEllipse (c.x - ri, c.y - ri, 2 * ri, 2 * ri);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (c.x - ri + 0.5f, c.y - ri + 0.5f, 2 * ri - 1, 2 * ri - 1, 1.0f);

    // Value and label.
    g.setColour (text);
    g.setFont (font (ri * 0.46f, true));
    g.drawText (format (getValue()), juce::Rectangle<float> (c.x - ri, c.y - ri * 0.42f, 2 * ri, ri * 0.52f), juce::Justification::centred);
    g.setColour (textDim);
    g.setFont (font (juce::jmax (11.0f, ri * 0.17f), true));
    g.drawText (label + (unit.isEmpty() ? juce::String() : "  " + unit), juce::Rectangle<float> (c.x - ri, c.y + ri * 0.16f, 2 * ri, ri * 0.28f),
                juce::Justification::centred);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showValueEditor();
    else
        Slider::mouseDown (e);
}

void Knob::showValueEditor()
{
    editor = std::make_unique<juce::TextEditor>();
    const int w = getWidth() / 2, h = 26;
    editor->setBounds (getLocalBounds().withSizeKeepingCentre (w, h));
    editor->setFont (theme::font (15.0f, true));
    editor->setJustification (juce::Justification::centred);
    editor->setText (format (getValue()), false);
    editor->selectAll();
    editor->onReturnKey = [this] {
        const double v = editor->getText().retainCharacters ("-0123456789.").getDoubleValue();
        setValue (snapValue (juce::jlimit (getMinimum(), getMaximum(), v), notDragging), juce::sendNotificationSync);
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Knob> (this)] { if (sp) sp->editor.reset(); });
    };
    editor->onEscapeKey = editor->onFocusLost = [this] {
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Knob> (this)] { if (sp) sp->editor.reset(); });
    };
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
}
