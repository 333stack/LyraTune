#include "KeyboardPanel.h"

static const char* kNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
static int whiteIndex (int pc) { static const int w[] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 }; return w[pc]; }

static float heatTop() { return 0.0f; }

juce::Rectangle<float> KeyboardPanel::labelRow() const
{
    const float y = float (2 * KeyboardPanel::kHeatH + 2 * KeyboardPanel::kHeatGap);
    return { 0.0f, y, float (getWidth()), float (kLabelH) };
}

juce::Rectangle<float> KeyboardPanel::piano() const
{
    const auto l = labelRow();
    return { 0.0f, l.getBottom() + 2.0f, float (getWidth()), float (getHeight()) - l.getBottom() - 2.0f };
}

juce::Rectangle<float> KeyboardPanel::keyRect (int pc) const
{
    const auto p = piano();
    const float cell = p.getWidth() / 12.0f;
    if (isBlack (pc))
        return { cell * float (pc) + cell * 0.08f, p.getY(), cell * 0.84f, p.getHeight() * 0.58f };
    const float ww = p.getWidth() / 7.0f;
    return { ww * float (whiteIndex (pc)), p.getY(), ww, p.getHeight() };
}

int KeyboardPanel::pitchClassAt (juce::Point<float> pt) const
{
    if (labelRow().contains (pt))
        return juce::jlimit (0, 11, int (pt.x / (float (getWidth()) / 12.0f)));
    if (! piano().contains (pt))
        return -1;
    for (int pc = 0; pc < 12; ++pc) // black keys sit on top: test them first
        if (isBlack (pc) && keyRect (pc).contains (pt)) return pc;
    for (int pc = 0; pc < 12; ++pc)
        if (! isBlack (pc) && keyRect (pc).contains (pt)) return pc;
    return -1;
}

void KeyboardPanel::addHeat (float inMidi, float outMidi, float w)
{
    auto splat = [w] (std::array<float, kBins>& h, float midi) {
        // Bin 0 is half a semitone below C, so each note's cell centre matches the label row.
        const float pos = std::fmod (midi + 0.5f, 12.0f) / 12.0f * float (kBins);
        const int c = int (pos);
        for (int k = -4; k <= 4; ++k)
        {
            const float d = (float (c + k) + 0.5f - pos) / 1.6f;
            h[size_t ((c + k + kBins) % kBins)] += w * std::exp (-d * d);
        }
    };
    if (inMidi > 0) splat (heatIn, inMidi);
    if (outMidi > 0) splat (heatOut, outMidi);
}

void KeyboardPanel::decayHeat (float f)
{
    for (auto* h : { &heatIn, &heatOut })
        for (auto& v : *h) v = v * f < 0.002f ? 0.0f : juce::jmin (4.0f, v * f);
    if (showHeat) repaint (0, 0, getWidth(), 2 * kHeatH + kHeatGap);
}

void KeyboardPanel::drawHeat (juce::Graphics& g, juce::Rectangle<float> r, const std::array<float, kBins>& h) const
{
    using namespace theme;
    g.setColour (bg0);
    g.fillRoundedRectangle (r, 3.0f);
    const float bw = r.getWidth() / float (kBins);
    for (int i = 0; i < kBins; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, h[size_t (i)] * 0.3f);
        if (v <= 0.0f) continue;
        const auto col = v < 0.7f ? violet.withAlpha (v / 0.7f) : violet.interpolatedWith (lavender.brighter (0.3f), (v - 0.7f) / 0.3f);
        g.setColour (col);
        g.fillRect (r.getX() + float (i) * bw, r.getY(), bw + 0.6f, r.getHeight());
    }
    g.setColour (line.withAlpha (0.6f));
    for (int k = 1; k < 12; ++k)
        g.fillRect (r.getX() + r.getWidth() * float (k) / 12.0f, r.getY() + 3.0f, 1.0f, r.getHeight() - 6.0f);
}

void KeyboardPanel::paint (juce::Graphics& g)
{
    using namespace theme;
    const float W = float (getWidth());

    if (showHeat)
    {
        drawHeat (g, { 0.0f, heatTop(), W, float (kHeatH) }, heatIn);
        drawHeat (g, { 0.0f, float (kHeatH + kHeatGap), W, float (kHeatH) }, heatOut);
    }

    const bool live = state.voiced && state.presence > 0.05f;
    const int detectedPc = live ? ((juce::roundToInt (state.inputMidi) % 12) + 12) % 12 : -1;
    const int targetPc = live && state.target >= 0 ? state.target % 12 : -1;

    // Label row.
    const auto lr = labelRow();
    const float cell = W / 12.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        auto r = juce::Rectangle<float> (cell * float (pc), lr.getY(), cell, lr.getHeight()).reduced (1.5f, 0.0f);
        const bool on = lyra::isAllowed (mask, pc);
        g.setColour (isBlack (pc) ? bg0 : bg2);
        g.fillRoundedRectangle (r, 3.0f);
        if (pc == hoverPc)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (r, 3.0f);
        }
        g.setColour (pc == rootPc ? lavender : (on ? text : textFaint));
        g.setFont (font (13.0f, true));
        g.drawText (kNames[pc], r, juce::Justification::centred);
        if (showDetected && pc == detectedPc) // detected input note: bright bar under the label
        {
            g.setColour (juce::Colours::white.withAlpha (state.presence));
            g.fillRoundedRectangle (r.removeFromBottom (3.0f).reduced (4.0f, 0.0f), 1.5f);
        }
    }

    // Piano: white keys, then black keys on top.
    auto drawKey = [&] (int pc) {
        const auto r = keyRect (pc).reduced (1.0f, 0.0f);
        const bool on = lyra::isAllowed (mask, pc);
        const bool black = isBlack (pc);
        // Piano look: allowed = white / near-black, forbidden = dimmed grey / deep black.
        juce::Colour top, bottom;
        if (black) { top = on ? juce::Colour (0xff3a3842) : juce::Colour (0xff141417); bottom = on ? juce::Colour (0xff1f1e24) : juce::Colour (0xff0b0b0d); }
        else       { top = on ? juce::Colour (0xfff7f6fb) : juce::Colour (0xff6d6c74); bottom = on ? juce::Colour (0xffdcd9e6) : juce::Colour (0xff55545b); }
        if (pc == hoverPc) { top = top.brighter (0.12f); bottom = bottom.brighter (0.12f); }
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 4.0f, 4.0f, false, false, true, true);
        if (black)
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 2 }).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        g.fillPath (p);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
        if (black && on) // thin top highlight so allowed sharps read as "lit"
        {
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (r.getX() + 2.0f, r.getY(), r.getWidth() - 4.0f, 2.0f);
        }
        if (showDetected && pc == targetPc) // output target: violet glow + outline
        {
            g.setColour (violet.withAlpha (0.30f * state.presence));
            g.fillPath (p);
            g.setColour (violet.withAlpha (state.presence));
            g.strokePath (p, juce::PathStrokeType (2.5f));
        }
        if (pc == rootPc) // root marker
        {
            const float d = 7.0f;
            g.setColour (violet);
            g.fillEllipse (r.getCentreX() - d * 0.5f, r.getBottom() - d - 8.0f, d, d);
        }
    };
    for (int pc = 0; pc < 12; ++pc) if (! isBlack (pc)) drawKey (pc);
    for (int pc = 0; pc < 12; ++pc) if (isBlack (pc)) drawKey (pc);

    if (mask == 0)
    {
        const auto p = piano();
        g.setColour (bg0.withAlpha (0.75f));
        g.fillRect (p);
        g.setColour (warn);
        g.setFont (font (15.0f, true));
        g.drawText ("No notes enabled - correction paused. Click a key to enable it.", p, juce::Justification::centred);
    }
}

void KeyboardPanel::mouseDown (const juce::MouseEvent& e)
{
    const int pc = pitchClassAt (e.position);
    if (pc < 0) return;
    if (e.mods.isPopupMenu()) { if (onSetRoot) onSetRoot (pc); }
    else if (onToggleNote) onToggleNote (pc);
}

void KeyboardPanel::mouseMove (const juce::MouseEvent& e)
{
    const int pc = pitchClassAt (e.position);
    if (pc != hoverPc) { hoverPc = pc; repaint(); }
}
