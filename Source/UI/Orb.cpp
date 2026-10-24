#include "Orb.h"

static const char* kNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

static juce::String noteName (int midi)
{
    return juce::String (kNames[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

// Angle on the circle for a pitch class (C at 12 o'clock, clockwise).
static float pcAngle (float midi) { return juce::MathConstants<float>::twoPi * std::fmod (midi, 12.0f) / 12.0f; }

Orb::Orb (juce::RangedAudioParameter& s, juce::RangedAudioParameter& a, juce::UndoManager* um)
    : speedParam (s), amountParam (a),
      speedAttach (s, [this] (float v) { speedNorm = speedParam.convertTo0to1 (v); repaint(); }, um),
      // Only Speed opens an undo transaction: one XY drag = one undo step (Amount joins it).
      amountAttach (a, [this] (float v) { amountNorm = amountParam.convertTo0to1 (v); repaint(); }, nullptr)
{
    speedAttach.sendInitialUpdate();
    amountAttach.sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
}

void Orb::setDisplay (const DisplayState& s, bool byp)
{
    state = s;
    bypassed = byp;
    repaint();
}

juce::Rectangle<float> Orb::padArea() const
{
    // Square inscribed in the halo: the handle stays inside the orb.
    const auto b = getLocalBounds().toFloat();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    return juce::Rectangle<float> (R * 1.30f, R * 1.30f).withCentre (b.getCentre());
}

juce::Point<float> Orb::handlePos() const
{
    const auto p = padArea();
    return { p.getX() + (1.0f - speedNorm) * p.getWidth(), p.getBottom() - amountNorm * p.getHeight() };
}

void Orb::renderBackground()
{
    using namespace theme;
    const float sc = juce::Component::getApproximateScaleFactorForComponent (this);
    background = juce::Image (juce::Image::ARGB, juce::jmax (1, int (getWidth() * sc)), juce::jmax (1, int (getHeight() * sc)), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (sc));
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    // Soft outer bloom.
    g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.10f), c.x, c.y, violet.withAlpha (0.0f), c.x + R, c.y, true));
    g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);

    // Dark disc.
    const float rd = R * 0.90f;
    g.setGradientFill (juce::ColourGradient (bg3, c.x - rd * 0.3f, c.y - rd * 0.4f, bg0, c.x + rd * 0.5f, c.y + rd * 0.7f, true));
    g.fillEllipse (c.x - rd, c.y - rd, 2 * rd, 2 * rd);

    // Grainy halo: many fine radial strokes with deterministic random length/alpha.
    juce::Random rng (7);
    for (int i = 0; i < 480; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * float (i) / 480.0f + rng.nextFloat() * 0.01f;
        const float r0 = rd * (0.95f + rng.nextFloat() * 0.03f), r1 = rd * (1.0f + rng.nextFloat() * 0.035f);
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.setColour (juce::Colours::white.withAlpha (0.02f + rng.nextFloat() * 0.07f));
        g.drawLine ({ c + d * r0, c + d * r1 }, 0.7f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (c.x - rd, c.y - rd, 2 * rd, 2 * rd, 1.0f);
}

void Orb::paint (juce::Graphics& g)
{
    using namespace theme;
    if (background.isNull()) renderBackground();
    g.drawImage (background, getLocalBounds().toFloat());

    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = juce::jmin (getWidth(), getHeight()) * 0.5f;
    const float pres = bypassed ? 0.0f : state.presence;
    const float level = juce::jlimit (0.0f, 1.0f, state.level);

    // Middle ring with glow; shifts toward pink as Speed goes aggressive (below 0 ms).
    const float rm = R * 0.58f;
    const float aggressive = juce::jlimit (0.0f, 1.0f, -speedParam.convertFrom0to1 (speedNorm) / 3.0f);
    const auto ringCol = violet.interpolatedWith (pink, 0.8f * aggressive);
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (ringCol.withAlpha (0.05f + 0.06f * pres + 0.05f * aggressive));
        g.drawEllipse (c.x - rm, c.y - rm, 2 * rm, 2 * rm, float (i) * 3.5f);
    }
    g.setColour (ringCol.interpolatedWith (lavender, 0.3f * pres).withAlpha (0.75f + 0.25f * pres));
    g.drawEllipse (c.x - rm, c.y - rm, 2 * rm, 2 * rm, 2.0f);

    // Core: wedge ring whose palette rotates with the output pitch class.
    const float rin = R * 0.18f, rout = R * 0.44f;
    const float rot = pcAngle (state.outputMidi);
    const float alpha = bypassed ? 0.15f : 0.30f + 0.55f * pres * (0.4f + 0.6f * level);
    constexpr int segs = 72;
    for (int i = 0; i < segs; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * float (i) / segs, a1 = a0 + juce::MathConstants<float>::twoPi / segs + 0.01f;
        const float u = std::fmod ((a0 + rot) / juce::MathConstants<float>::twoPi + 1.0f, 1.0f);
        const float tri = u < 0.5f ? u * 2.0f : 2.0f - u * 2.0f; // 0 -> 1 -> 0 around the circle
        juce::Path p;
        p.addPieSegment (c.x - rout, c.y - rout, 2 * rout, 2 * rout, a0, a1, rin / rout);
        g.setColour (accentAt (tri).withAlpha (alpha));
        g.fillPath (p);
    }
    g.setGradientFill (juce::ColourGradient (bg0.withAlpha (0.0f), c.x, c.y, bg0.withAlpha (0.55f), c.x + rout, c.y, true));
    g.fillEllipse (c.x - rout, c.y - rout, 2 * rout, 2 * rout);

    // Doubler: two satellite voices beside the core; spread = width, size/brightness = blend.
    const float dblTarget = (dblOn && ! bypassed) ? 1.0f : 0.0f;
    dblFade += (dblTarget - dblFade) * 0.15f; // eased fade in/out at the editor frame rate
    if (dblFade > 0.01f)
    {
        const float tsec = float (juce::Time::getMillisecondCounter() % 100000) * 0.001f;
        for (int side : { -1, 1 })
        {
            const float wob = 0.03f * std::sin (tsec * (side < 0 ? 1.7f : 2.3f));
            const float dist = rm * (0.82f + 0.34f * dblWidth) * (1.0f + wob);
            const float rr = R * (0.045f + 0.045f * dblBlend) * (0.85f + 0.3f * state.level * pres);
            const juce::Point<float> p (c.x + float (side) * dist, c.y + R * 0.02f * std::sin (tsec * 1.3f + float (side)));
            const auto col = side < 0 ? pink : blue;
            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.85f * dblFade), p.x, p.y, col.withAlpha (0.0f), p.x + rr * 2.6f, p.y, true));
            g.fillEllipse (p.x - rr * 2.6f, p.y - rr * 2.6f, rr * 5.2f, rr * 5.2f);
            g.setColour (col.interpolatedWith (juce::Colours::white, 0.5f).withAlpha ((0.75f + 0.25f * dblBlend) * dblFade));
            g.fillEllipse (p.x - rr, p.y - rr, 2 * rr, 2 * rr);
        }
    }

    // Correction arc: detected pitch -> corrected pitch on the middle ring.
    if (pres > 0.01f && state.voiced)
    {
        const float aIn = pcAngle (state.inputMidi), aOut = pcAngle (state.outputMidi);
        float delta = aOut - aIn;
        if (delta > juce::MathConstants<float>::pi) delta -= juce::MathConstants<float>::twoPi;
        if (delta < -juce::MathConstants<float>::pi) delta += juce::MathConstants<float>::twoPi;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rm, rm, 0.0f, aIn, aIn + delta, true);
        g.setColour (pink.withAlpha (0.9f * pres));
        g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        auto dot = [&] (float a, float r, juce::Colour col) {
            const juce::Point<float> p (c.x + rm * std::sin (a), c.y - rm * std::cos (a));
            g.setColour (col);
            g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
        };
        dot (aIn, 4.0f, textDim.withAlpha (pres));
        dot (aOut, 5.5f, juce::Colours::white.withAlpha (pres));
    }

    // Centre readout.
    g.setColour (text.withAlpha (bypassed ? 0.35f : 0.35f + 0.65f * pres));
    if (bypassed)
    {
        g.setFont (font (R * 0.11f, true));
        g.drawText ("BYPASSED", juce::Rectangle<float> (R, R * 0.3f).withCentre (c), juce::Justification::centred);
    }
    else if (state.pending >= 0 && pres > 0.01f) // Note Stabilizer is confirming a new note
    {
        g.setColour (lavender.withAlpha (0.55f * pres));
        g.setFont (font (R * 0.16f, true));
        g.drawText (noteName (state.pending), juce::Rectangle<float> (R, R * 0.22f).withCentre (c.translated (0, -R * 0.03f)), juce::Justification::centred);
        g.setFont (font (R * 0.06f, true));
        g.drawText ("confirming", juce::Rectangle<float> (R, R * 0.1f).withCentre (c.translated (0, R * 0.11f)), juce::Justification::centred);
    }
    else if (state.target >= 0 && pres > 0.01f)
    {
        g.setFont (font (R * 0.16f, true));
        g.drawText (noteName (state.target), juce::Rectangle<float> (R, R * 0.22f).withCentre (c.translated (0, -R * 0.03f)), juce::Justification::centred);
        const int cents = juce::roundToInt ((state.inputMidi - float (state.target)) * 100.0f);
        g.setFont (font (R * 0.065f));
        g.setColour (textDim.withAlpha (pres));
        g.drawText ((cents > 0 ? "+" : "") + juce::String (cents) + " ct", juce::Rectangle<float> (R, R * 0.1f).withCentre (c.translated (0, R * 0.11f)),
                    juce::Justification::centred);
    }
    else
    {
        g.setFont (font (R * 0.12f, true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")), juce::Rectangle<float> (R, R * 0.2f).withCentre (c), juce::Justification::centred);
    }

    // XY handle.
    const auto hp = handlePos();
    const float hr = (dragging || hoverHandle) ? 9.0f : 7.5f;
    juce::Path hpath;
    hpath.addEllipse (hp.x - hr, hp.y - hr, 2 * hr, 2 * hr);
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 8, {}).drawForPath (g, hpath);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d7e0), hp.x, hp.y - hr, juce::Colour (0xff8c8a96), hp.x, hp.y + hr, false));
    g.fillPath (hpath);
    if (dragging || hoverHandle)
    {
        g.setColour (text);
        g.setFont (font (12.0f, true));
        const auto label = "Speed " + speedParam.getCurrentValueAsText() + "   Amount " + amountParam.getCurrentValueAsText();
        auto box = juce::Rectangle<float> (180.0f, 22.0f).withCentre (hp.translated (0, hp.y > c.y ? -24.0f : 24.0f));
        box = box.constrainedWithin (getLocalBounds().toFloat());
        g.setColour (bg3.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (text);
        g.drawText (label, box, juce::Justification::centred);
    }
}

void Orb::mouseMove (const juce::MouseEvent& e)
{
    const bool h = e.position.getDistanceFrom (handlePos()) < 14.0f;
    if (h != hoverHandle) { hoverHandle = h; repaint(); }
    setMouseCursor (h ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void Orb::mouseDown (const juce::MouseEvent& e)
{
    if (e.position.getDistanceFrom (handlePos()) > 14.0f)
        return;
    dragging = true;
    speedAttach.beginGesture();
    amountAttach.beginGesture();
    mouseDrag (e);
}

void Orb::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto p = padArea();
    speedNorm = 1.0f - juce::jlimit (0.0f, 1.0f, (e.position.x - p.getX()) / p.getWidth()); // right = faster
    amountNorm = juce::jlimit (0.0f, 1.0f, (p.getBottom() - e.position.y) / p.getHeight());
    speedAttach.setValueAsPartOfGesture (speedParam.convertFrom0to1 (speedNorm));
    amountAttach.setValueAsPartOfGesture (amountParam.convertFrom0to1 (amountNorm));
    repaint();
}

void Orb::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    speedAttach.endGesture();
    amountAttach.endGesture();
    repaint();
}

void Orb::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.position.getDistanceFrom (handlePos()) > 14.0f) return;
    speedAttach.setValueAsCompleteGesture (speedParam.convertFrom0to1 (speedParam.getDefaultValue()));
    amountAttach.setValueAsCompleteGesture (amountParam.convertFrom0to1 (amountParam.getDefaultValue()));
}
