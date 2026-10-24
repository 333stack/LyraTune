#pragma once
#include "PluginProcessor.h"
#include "UI/KeyboardPanel.h"
#include "UI/Knob.h"
#include "UI/LyraLookAndFeel.h"
#include "UI/Orb.h"
#include "UI/Widgets.h"

// Fixed 900x600 design canvas; the editor scales it uniformly (75-150 %) so the layout is designed
// once, never squeezed. All visual data comes from the processor's telemetry queue.
class LyraTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kW = 900, kH = 600;

    explicit LyraTuneEditor (LyraTuneProcessor&);
    ~LyraTuneEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (theme::bg1); }

private:
    struct Canvas : public juce::Component
    {
        explicit Canvas (LyraTuneEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        // Open on mouse-up: opening on mouse-down let the same click's mouse-up dismiss the menu.
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (owner.rootArea.contains (e.getPosition()) && owner.rootArea.contains (e.getMouseDownPosition()))
                owner.toggleRootMenu();
        }
        LyraTuneEditor& owner;
    };

    void timerCallback() override;
    void layout();
    void toggleNote (int pc);
    void setRoot (int pc);
    void setParam (const juce::String& id, float value);
    void toggleRootMenu(); // click root area: open; click it again: close
    void showRootMenu();
    void showMainMenu();
    void setScale (float s);
    void updateTooltips();

    LyraTuneProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    LyraLookAndFeel lnf;
    Canvas canvas { *this };

    Knob speedKnob, amountKnob, sustainKnob;
    ValueStrip refStrip, widthStrip, blendStrip;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttach, amountAttach, sustainAttach, refAttach, widthAttach, blendAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach, doublerAttach, strikeAttach;
    Orb orb;
    KeyboardPanel keyboard;
    SegmentedControl scaleSeg, stabilizerSeg, rangeSeg;

    IconButton menuButton { "menu", icons::menu() };
    IconButton undoButton { "undo", icons::undo() }, redoButton { "redo", icons::redo() }, resetButton { "reset", icons::reset() };
    PillButton aButton { "A" }, bButton { "B" };
    IconButton copyButton { "copy", icons::copyArrow() };
    IconButton helpButton { "tips", icons::help() };
    IconButton heatButton { "heat", icons::eye(), "In/Out HeatMaps" };
    IconButton detectedButton { "detected", icons::notes(), "Detected Notes" };
    IconButton bypassButton { "bypass", icons::power(), "Global Bypass" };
    IconButton doublerButton { "doubler", icons::power(), "Doubler" };
    IconButton strikeButton { "strike", icons::bolt(), "LyraStrike" };
    juce::HyperlinkButton credit { "made with <3 by stack", juce::URL ("https://soundcloud.com/222stack") };
    juce::Rectangle<int> rootArea;
    bool rootMenuOpen = false;
    double rootMenuClosedByAreaClickMs = -1e9; // a click on the root area that dismissed the menu
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    DisplayState display;
    double lastVoicedMs = -1e9;
    float latestRms = 0;
    juce::String latencyText;
    bool lastDoublerOn = false, lastStrikeOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyraTuneEditor)
};
