#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Visual tokens shared by every component. Change colours here, never inline.
namespace theme
{
inline const juce::Colour bg0 { 0xff0f0f12 };   // deepest (strips, wells)
inline const juce::Colour bg1 { 0xff17171b };   // window
inline const juce::Colour bg2 { 0xff1f1f25 };   // raised panels
inline const juce::Colour bg3 { 0xff2a2a32 };   // controls, hover
inline const juce::Colour line { 0xff34343e };
inline const juce::Colour text { 0xffeeecf4 };
inline const juce::Colour textDim { 0xff9a98a6 };
inline const juce::Colour textFaint { 0xff5f5d6a };

inline const juce::Colour violet { 0xff8d6dff };
inline const juce::Colour pink { 0xffff70b3 };
inline const juce::Colour blue { 0xff6aa8ff };
inline const juce::Colour lavender { 0xffbcaeff };
inline const juce::Colour warn { 0xffffb35c };

// Position 0..1 along the signature pink -> violet -> blue gradient.
inline juce::Colour accentAt (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    return t < 0.5f ? pink.interpolatedWith (violet, t * 2.0f) : violet.interpolatedWith (blue, (t - 0.5f) * 2.0f);
}

juce::Font font (float height, bool semibold = false);
} // namespace theme
