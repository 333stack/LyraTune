#include "Theme.h"
#include "BinaryData.h"

namespace theme
{
juce::Font font (float height, bool semibold)
{
    // Embedded Inter (SIL OFL): identical rendering regardless of fonts present in the Wine prefix.
    static const auto regular = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    static const auto bold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    return juce::Font (juce::FontOptions (semibold ? bold : regular).withHeight (height));
}
void drawHairline (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float alpha)
{
    const auto core = juce::Colours::white.interpolatedWith (lavender, 0.45f);
    juce::ColourGradient grad (core.withAlpha (0.0f), a, core.withAlpha (0.0f), b, false);
    grad.addColour (0.5, core.withAlpha (alpha));
    g.setGradientFill (grad);
    const bool vertical = std::abs (b.x - a.x) < std::abs (b.y - a.y);
    g.fillRect (vertical ? juce::Rectangle<float> (a.x - 0.5f, juce::jmin (a.y, b.y), 1.0f, std::abs (b.y - a.y))
                         : juce::Rectangle<float> (juce::jmin (a.x, b.x), a.y - 0.5f, std::abs (b.x - a.x), 1.0f));
}

void drawSectionPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (juce::Colours::white.withAlpha (0.014f));
    g.fillRoundedRectangle (r, radius);
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.075f), r.getX(), r.getY(),
                              juce::Colours::white.withAlpha (0.012f), r.getX(), r.getBottom(), false);
    g.setGradientFill (rim);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void drawSelectionGlow (juce::Graphics& g, juce::Rectangle<float> p, juce::Colour tint, float strength)
{
    const float rad = p.getHeight() * 0.5f;
    // Halo: three faint, widening rounded rects (a cheap blur).
    for (int i = 3; i >= 1; --i)
    {
        const float grow = 1.6f * float (i);
        g.setColour (tint.withAlpha (0.030f * strength));
        g.fillRoundedRectangle (p.expanded (grow), rad + grow);
    }
    // Tint over whatever pill fill is already there.
    g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.20f * strength), p.getX(), p.getY(),
                                             tint.withAlpha (0.07f * strength), p.getX(), p.getBottom(), false));
    g.fillRoundedRectangle (p, rad);
    // Edge and a glassy highlight across the top.
    g.setColour (tint.interpolatedWith (juce::Colours::white, 0.4f).withAlpha (0.22f * strength));
    g.drawRoundedRectangle (p.reduced (0.5f), rad, 1.0f);
    drawHairline (g, { p.getX() + rad, p.getY() + 1.5f }, { p.getRight() - rad, p.getY() + 1.5f }, 0.35f * strength);
}
} // namespace theme
