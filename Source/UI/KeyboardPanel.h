#pragma once
#include "DisplayState.h"
#include "Theme.h"
#include "../DSP/Pitch/Scale.h"
#include <array>
#include <functional>

// Input/Output HeatMaps + note label row + one-octave piano, all sharing one pitch-class x axis
// (12 equal cells, cell centre = note). Shows the allowed-note mask, root, detected input note and
// output target. Click a note to toggle it; right-click a note to make it the root.
class KeyboardPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kHeatH = 12, kHeatGap = 4, kLabelH = 26;
    static constexpr int kBins = 240; // heat resolution: 5 cents per bin

    std::function<void (int pitchClass)> onToggleNote, onSetRoot;

    void setMask (lyra::PitchClassMask m, int root) { if (m != mask || root != rootPc) { mask = m; rootPc = root; repaint(); } }
    void setShowHeat (bool b) { showHeat = b; repaint(); }
    void setShowDetected (bool b) { showDetected = b; repaint(); }
    void addHeat (float inputMidi, float outputMidi, float weight); // per telemetry sample
    void decayHeat (float factor);                                  // per frame
    void setDisplay (const DisplayState& s) { state = s; repaint(); }

    int heatInputY() const noexcept { return 0; }
    int heatOutputY() const noexcept { return kHeatH + kHeatGap; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverPc = -1; repaint(); }

private:
    int pitchClassAt (juce::Point<float>) const;
    juce::Rectangle<float> labelRow() const;
    juce::Rectangle<float> piano() const;
    juce::Rectangle<float> keyRect (int pc) const;
    void drawHeat (juce::Graphics&, juce::Rectangle<float>, const std::array<float, kBins>&) const;

    lyra::PitchClassMask mask = lyra::kAllNotes;
    int rootPc = 0, hoverPc = -1;
    bool showHeat = true, showDetected = true;
    std::array<float, kBins> heatIn {}, heatOut {};
    DisplayState state;
};
