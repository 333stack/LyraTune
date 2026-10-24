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
} // namespace theme
