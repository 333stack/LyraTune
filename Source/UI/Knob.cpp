#include "Knob.h"

static constexpr float kStartAngle = juce::MathConstants<float>::pi * -0.75f; // 7:30 o'clock
static constexpr float kEndAngle = juce::MathConstants<float>::pi * 0.75f;    // 4:30 o'clock
static constexpr int kTicks = 57; // odd: a centre tick for bipolar knobs

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

void Knob::setOverloadDetent (double value, double snapRange)
{
    hasDetent = true;
    detent = value;
    detentSnap = snapRange;
    wasOverloaded = getValue() < detent;
    if (wasOverloaded) startTimerHz (60);
}

double Knob::snapValue (double attempted, DragMode mode)
{
    if (hasDetent && mode != notDragging && std::abs (attempted - detent) < detentSnap)
        return detent;
    return attempted;
}

float Knob::overloadAmount() const
{
    if (! hasDetent || getValue() >= detent) return 0.0f;
    return juce::jlimit (0.0f, 1.0f, float ((detent - getValue()) / (detent - getMinimum())));
}

void Knob::valueChanged()
{
    if (! hasDetent) return;
    const bool over = getValue() < detent;
    if (over && ! wasOverloaded) burst = 1.0f; // crossing into overload: shockwave
    wasOverloaded = over;
    if ((over || burst > 0.0f) && ! isTimerRunning()) startTimerHz (60);
}

void Knob::timerCallback()
{
    phase += 1.0f / 60.0f;
    burst = juce::jmax (0.0f, burst - 1.0f / (0.35f * 60.0f)); // 350 ms shockwave
    if (overloadAmount() <= 0.0f && burst <= 0.0f) { stopTimer(); phase = 0; }
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f - glowMargin;
    auto c = bounds.getCentre();
    const float t = float (valueToProportionOfLength (getValue()));
    const bool hot = isMouseOverOrDragging() || hasKeyboardFocus (false);
    const float amt = overloadAmount();
    const juce::Colour heat (0xffff3d5e);

    // Overload glow behind the dial (pulses faster and brighter the deeper it goes).
    if (amt > 0.0f || burst > 0.0f)
    {
        const float pulse = 0.75f + 0.25f * std::sin (phase * (8.0f + 10.0f * amt));
        const float a = juce::jlimit (0.0f, 1.0f, 0.55f * amt * pulse + 0.5f * burst);
        const float rg = R + glowMargin;
        g.setGradientFill (juce::ColourGradient (heat.withAlpha (a), c.x, c.y, heat.withAlpha (0.0f), c.x + rg, c.y, true));
        g.fillEllipse (c.x - rg, c.y - rg, 2 * rg, 2 * rg);
    }
    // Tremble: tiny, fast, deterministic offset proportional to overload depth.
    if (amt > 0.0f)
        c += juce::Point<float> (std::sin (phase * 71.0f), std::cos (phase * 53.0f)) * (1.4f * amt);

    // Outer well with soft shadow.
    juce::Path well;
    well.addEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), int (R * 0.12f), { 0, int (R * 0.04f) }).drawForPath (g, well);
    g.setGradientFill (juce::ColourGradient (bg2, c.x, c.y - R, bg0, c.x, c.y + R, false));
    g.fillPath (well);

    // Tick ring: lit ticks follow the accent gradient up to the value; overloaded ticks run hot.
    const float r1 = R * 0.80f, r2 = R * 0.93f, tickW = juce::jmax (1.6f, R * 0.028f);
    const int lit = juce::roundToInt (t * (kTicks - 1));
    const float detentT = hasDetent ? float (valueToProportionOfLength (detent)) : 2.0f;
    const bool overZoneHigh = hasDetent && float (valueToProportionOfLength (getMinimum())) > detentT;
    for (int i = 0; i < kTicks; ++i)
    {
        const float u = float (i) / float (kTicks - 1);
        const float a = kStartAngle + (kEndAngle - kStartAngle) * u;
        const juce::Point<float> dir (std::sin (a), -std::cos (a));
        const int mid = (kTicks - 1) / 2;
        const bool on = bipolar ? (lit >= mid ? (i >= mid && i <= lit) : (i >= lit && i <= mid)) : i <= lit;
        juce::Colour col = on ? accentAt (u) : line.brighter (hot ? 0.25f : 0.0f);
        float ext = 0.0f;
        const bool inOverZone = hasDetent && (overZoneHigh ? u > detentT : u < detentT);
        if (on && inOverZone && amt > 0.0f)
        {
            // Per-tick flicker from a cheap hash of tick index and time.
            const float n = 0.5f + 0.5f * std::sin (float (i) * 12.9898f + phase * (23.0f + 3.0f * float (i % 5)));
            col = heat.interpolatedWith (juce::Colours::white, 0.15f + 0.55f * n * amt);
            ext = R * 0.05f * n * amt;
        }
        if (i == lit)
        {
            col = juce::Colours::white;
            ext = juce::jmax (ext, R * 0.035f);
        }
        g.setColour (col);
        g.drawLine ({ c + dir * (r1 - ext), c + dir * (r2 + ext) }, i == lit ? tickW * 1.6f : tickW);
    }

    // Detent marker: a notch just outside the ring.
    if (hasDetent)
    {
        const float a = kStartAngle + (kEndAngle - kStartAngle) * detentT;
        const juce::Point<float> dir (std::sin (a), -std::cos (a)), perp (-dir.y, dir.x);
        const auto tip = c + dir * (r2 + R * 0.03f);
        juce::Path notch;
        notch.addTriangle (tip, tip + dir * (R * 0.07f) + perp * (R * 0.035f), tip + dir * (R * 0.07f) - perp * (R * 0.035f));
        g.setColour (amt > 0.0f ? heat.interpolatedWith (juce::Colours::white, 0.5f) : text.withAlpha (0.85f));
        g.fillPath (notch);
    }

    // Shockwave on crossing into overload.
    if (burst > 0.0f)
    {
        const float rr = R * (0.95f + 0.45f * (1.0f - burst));
        g.setColour (heat.withAlpha (burst));
        g.drawEllipse (c.x - rr, c.y - rr, 2 * rr, 2 * rr, 2.0f + 3.0f * burst);
    }

    // Raised inner disc.
    const float ri = R * 0.70f;
    g.setGradientFill (juce::ColourGradient (bg3.brighter (0.08f), c.x, c.y - ri, bg1, c.x, c.y + ri, false));
    g.fillEllipse (c.x - ri, c.y - ri, 2 * ri, 2 * ri);
    g.setColour (amt > 0.0f ? heat.withAlpha (0.25f + 0.35f * amt) : juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (c.x - ri + 0.5f, c.y - ri + 0.5f, 2 * ri - 1, 2 * ri - 1, amt > 0.0f ? 1.5f : 1.0f);

    // Value (glows hot when overloaded) and label.
    const auto valueBox = juce::Rectangle<float> (c.x - ri, c.y - ri * 0.42f, 2 * ri, ri * 0.52f);
    g.setFont (font (ri * 0.46f, true));
    if (amt > 0.0f)
    {
        g.setColour (heat.withAlpha (0.35f * amt));
        for (auto off : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
            g.drawText (format (getValue()), valueBox + off, juce::Justification::centred);
    }
    g.setColour (amt > 0.0f ? juce::Colours::white.interpolatedWith (heat, 0.25f * amt) : text);
    g.drawText (format (getValue()), valueBox, juce::Justification::centred);
    g.setColour (amt > 0.0f ? heat.interpolatedWith (textDim, 0.4f) : textDim);
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
