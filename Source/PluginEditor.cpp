#include "PluginEditor.h"
#include "Parameters.h"

namespace layout
{
// Design-canvas coordinates (900 x 600).
constexpr int headerH = 44;
constexpr int rowY = 372, rowH = 40;
// Section wells (drawn by the canvas; controls are laid out inside them).
const juce::Rectangle<float> scaleWell { 6.0f, rowY + 4.0f, 322.0f, rowH - 8.0f };
const juce::Rectangle<float> stabilizerWell { 334.0f, rowY + 4.0f, 290.0f, rowH - 8.0f };
const juce::Rectangle<float> rangeWell { 630.0f, rowY + 4.0f, 264.0f, rowH - 8.0f };
const juce::Rectangle<float> doublerFrame { 8.0f, 52.0f, 114.0f, 140.0f };
const juce::Rectangle<float> latencyFrame { 8.0f, rowY - 42.0f, 224.0f, 38.0f };
constexpr int keysX = 160, keysW = 600, keysY = 420, keysBottom = 592;
constexpr int rightX = 772;
} // namespace layout

static juce::String formatSpeed (double v) { return std::abs (v) < 10.0 ? juce::String (v, 1) : juce::String (juce::roundToInt (v)); }

LyraTuneEditor::LyraTuneEditor (LyraTuneProcessor& p)
    : AudioProcessorEditor (p), proc (p), state (p.apvts),
      speedKnob ("Speed", "ms", formatSpeed, true),
      amountKnob ("Amount", "%", [] (double v) { return juce::String (juce::roundToInt (v)); }),
      sustainKnob ("Sustain", {}, [] (double v) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i); }),
      orb (*state.getParameter (ParamID::speed), *state.getParameter (ParamID::amount), &p.undoManager),
      scaleSeg (*state.getParameter (ParamID::scale), { "Chromatic", "Major", "Minor", "Custom" }, &p.undoManager),
      stabilizerSeg (*state.getParameter (ParamID::stabilizer), kStabilizerNames, &p.undoManager),
      rangeSeg (*state.getParameter (ParamID::range), { "Low", "Mid", "High" }, &p.undoManager)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);

    for (auto* c : std::initializer_list<juce::Component*> { &orb, &speedKnob, &amountKnob, &sustainKnob, &scaleSeg, &stabilizerSeg, &rangeSeg, &keyboard, &refStrip,
                                                            &menuButton, &undoButton, &redoButton, &resetButton, &aButton, &copyButton, &bButton,
                                                            &helpButton, &heatButton, &detectedButton, &bypassButton, &credit,
                                                            &doublerButton, &widthStrip, &blendStrip, &hqButton })
        canvas.addAndMakeVisible (c);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    speedAttach = std::make_unique<SA> (state, ParamID::speed, speedKnob);
    amountAttach = std::make_unique<SA> (state, ParamID::amount, amountKnob);
    sustainAttach = std::make_unique<SA> (state, ParamID::sustain, sustainKnob);
    sustainKnob.setBipolar (true);
    sustainKnob.setDoubleClickReturnValue (true, 0.0);
    refAttach = std::make_unique<SA> (state, ParamID::referenceHz, refStrip);
    widthAttach = std::make_unique<SA> (state, ParamID::doublerWidth, widthStrip);
    blendAttach = std::make_unique<SA> (state, ParamID::doublerBlend, blendStrip);
    for (auto* st : { &widthStrip, &blendStrip }) st->fillFromLeft = true;
    widthStrip.setDoubleClickReturnValue (true, 70.0);
    blendStrip.setDoubleClickReturnValue (true, 50.0);
    doublerButton.setClickingTogglesState (true);
    hqButton.setClickingTogglesState (true);
    doublerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::doubler, doublerButton);
    hqAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::hq, hqButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.onColour = theme::pink;
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::bypass, bypassButton);
    speedKnob.setOverloadDetent (0.0, 0.4); // 0 ms notch; below it = aggressive mode, knob "overloads"
    speedKnob.setGlowMargin (22.0f);
    for (auto* k : { &speedKnob, &amountKnob })
        k->setDoubleClickReturnValue (true, k == &speedKnob ? 20.0 : 100.0);
    refStrip.setDoubleClickReturnValue (true, 440.0);

    credit.setFont (theme::font (10.0f), false, juce::Justification::centredRight);
    credit.setColour (juce::HyperlinkButton::textColourId, theme::textFaint);
    credit.setTooltip ("soundcloud.com/222stack");

    keyboard.onToggleNote = [this] (int pc) { toggleNote (pc); };
    keyboard.onSetRoot = [this] (int pc) { setRoot (pc); };

    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    resetButton.onClick = [this] { proc.resetAllParameters(); };
    aButton.onClick = [this] { if (proc.getABSlot() != 0) proc.switchAB(); };
    bButton.onClick = [this] { if (proc.getABSlot() != 1) proc.switchAB(); };
    copyButton.onClick = [this] { proc.copyToOtherAB(); };
    menuButton.onClick = [this] { showMainMenu(); };

    for (auto* b : { &helpButton, &heatButton, &detectedButton })
        b->setClickingTogglesState (true);
    helpButton.setToggleState (proc.ui.tooltips, juce::dontSendNotification);
    heatButton.setToggleState (proc.ui.heatmaps, juce::dontSendNotification);
    detectedButton.setToggleState (proc.ui.detectedNotes, juce::dontSendNotification);
    helpButton.onClick = [this] { proc.ui.tooltips = helpButton.getToggleState(); updateTooltips(); };
    heatButton.onClick = [this] { proc.ui.heatmaps = heatButton.getToggleState(); keyboard.setShowHeat (proc.ui.heatmaps); canvas.repaint(); };
    detectedButton.onClick = [this] { proc.ui.detectedNotes = detectedButton.getToggleState(); keyboard.setShowDetected (proc.ui.detectedNotes); };
    keyboard.setShowHeat (proc.ui.heatmaps);
    keyboard.setShowDetected (proc.ui.detectedNotes);

    speedKnob.setTooltip ("Retune speed. Clockwise = harder. Below 0 = aggressive.");
    amountKnob.setTooltip ("How much correction is applied.");
    sustainKnob.setTooltip ("+ tightens held notes, - lets them breathe.");
    stabilizerSeg.setTooltip ("New notes must hold this long before tuning. No added latency.");
    orb.setTooltip ("Live pitch. Drag the dot: right = faster, up = more amount.");
    keyboard.setTooltip ("Click: toggle note. Right-click: set root.");
    scaleSeg.setTooltip ("Scale for the root note.");
    rangeSeg.setTooltip ("Lowest note tracked. Higher = less latency.");
    refStrip.setTooltip ("A4 reference. Double-click: 440 Hz.");
    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    resetButton.setTooltip ("Reset all");
    aButton.setTooltip ("Snapshot A");
    bButton.setTooltip ("Snapshot B");
    copyButton.setTooltip ("Copy to other snapshot");
    helpButton.setTooltip ("Tooltips on/off");
    heatButton.setTooltip ("Show HeatMaps");
    detectedButton.setTooltip ("Show detected notes");
    bypassButton.setTooltip ("Bypass");
    doublerButton.setTooltip ("Two extra voices for a wider, thicker vocal.");
    widthStrip.setTooltip ("Doubler stereo spread.");
    blendStrip.setTooltip ("Doubler level.");
    hqButton.setTooltip ("Tighter tracking. +2 ms latency.");
    menuButton.setTooltip ("Settings");

    updateTooltips();
    // Capture the saved scale first: setResizeLimits() clamps the still-0x0 editor to the minimum size,
    // and resized() would then overwrite ui.scale with 0.75.
    const float initialScale = proc.ui.scale;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (double (kW) / kH);
    setResizeLimits (kW * 3 / 4, kH * 3 / 4, kW * 3 / 2, kH * 3 / 2);
    setSize (juce::roundToInt (kW * initialScale), juce::roundToInt (kH * initialScale));
    startTimerHz (60);
}

LyraTuneEditor::~LyraTuneEditor()
{
    tooltipWindow.reset();
    setLookAndFeel (nullptr);
}

void LyraTuneEditor::updateTooltips()
{
    if (proc.ui.tooltips && tooltipWindow == nullptr)
    {
        tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 700);
        tooltipWindow->setLookAndFeel (&lnf);
    }
    else if (! proc.ui.tooltips)
        tooltipWindow.reset();
}

void LyraTuneEditor::resized()
{
    const float s = float (getWidth()) / float (kW);
    proc.ui.scale = s;
    canvas.setBounds (0, 0, kW, kH);
    canvas.setTransform (juce::AffineTransform::scale (s));
    layout();
}

void LyraTuneEditor::layout()
{
    using namespace layout;
    menuButton.setBounds (10, 8, 28, 28);
    undoButton.setBounds (632, 8, 28, 28);
    redoButton.setBounds (662, 8, 28, 28);
    resetButton.setBounds (692, 8, 28, 28);
    aButton.setBounds (734, 6, 32, 32);
    copyButton.setBounds (766, 8, 28, 28);
    bButton.setBounds (794, 6, 32, 32);
    helpButton.setBounds (858, 8, 28, 28);

    orb.setBounds (juce::Rectangle<int> (310, 310).withCentre ({ 278, 208 }));
    speedKnob.setBounds (juce::Rectangle<int> (248, 248).withCentre ({ 736, 160 })); // 204 dial + 22 glow margin
    amountKnob.setBounds (juce::Rectangle<int> (112, 112).withCentre ({ 818, 306 }));
    sustainKnob.setBounds (juce::Rectangle<int> (98, 98).withCentre ({ 652, 312 }));

    // Doubler block, top-left of the orb panel (added after the orb, so it sits on top of its bounds).
    // Section frames are drawn by the canvas from the same rectangles (layout::doublerFrame etc.).
    doublerButton.setBounds (14, 56, 102, 28);
    widthStrip.setBounds (18, 112, 96, 26);
    blendStrip.setBounds (18, 158, 96, 26);
    hqButton.setBounds (176, rowY - 38, 44, 30);

    scaleSeg.setBounds (58, rowY + 5, 266, rowH - 10);
    stabilizerSeg.setBounds (408, rowY + 5, 212, rowH - 10);
    rangeSeg.setBounds (680, rowY + 5, 207, rowH - 10);

    keyboard.setBounds (keysX, keysY, keysW, keysBottom - keysY);
    rootArea = { 12, 470, 136, 116 };

    heatButton.setBounds (rightX, 420, 120, 30);
    detectedButton.setBounds (rightX, 454, 120, 30);
    bypassButton.setBounds (rightX, 488, 120, 30);
    refStrip.setBounds (rightX + 8, 546, 104, 30);
    credit.setBounds (rightX - 10, 581, 112, 14); // stays clear of the resize corner
}

void LyraTuneEditor::Canvas::paint (juce::Graphics& g)
{
    using namespace theme;
    using namespace layout;
    auto& e = owner;
    g.fillAll (bg1);

    // Header.
    g.setGradientFill (juce::ColourGradient (bg2.brighter (0.04f), 0, 0, bg2, 0, float (headerH), false));
    g.fillRect (0, 0, kW, headerH);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (0, headerH, kW, 1);
    // Wordmark: original "lyraTune" logotype in a soft capsule.
    const auto cap = juce::Rectangle<float> (360, 9, 180, 26);
    g.setColour (bg0.withAlpha (0.6f));
    g.fillRoundedRectangle (cap, 13.0f);
    juce::AttributedString word;
    word.append ("lyra", font (17.0f), textDim);
    word.append ("Tune", font (17.0f, true), text);
    word.setJustification (juce::Justification::centred);
    word.draw (g, cap);
    // Small mark next to the menu button: a gradient ring.
    g.setGradientFill (juce::ColourGradient (pink, 48, 12, blue, 66, 32, false));
    g.drawEllipse (48, 13, 18, 18, 3.0f);

    // Main area: left vignette, right panel with a curved edge.
    g.setGradientFill (juce::ColourGradient (bg2.withAlpha (0.6f), 278, 208, bg1, 0, 400, true));
    g.fillRect (0, headerH + 1, kW, rowY - headerH - 1);
    juce::Path right;
    right.addEllipse (560 - 30, 208 - 560 + 30, 1120 + 60, 1120 - 60); // big ellipse: only its left edge is visible
    juce::Path clip;
    clip.addRectangle (0.0f, float (headerH + 1), float (kW), float (rowY - headerH - 1));
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (bg2.brighter (0.03f), 600, float (headerH), bg1.darker (0.1f), 900, float (rowY), false));
        g.fillPath (right);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.strokePath (right, juce::PathStrokeType (1.0f));
    }

    // Mode row.
    g.setColour (bg0.withAlpha (0.7f));
    g.fillRect (0, rowY, kW, rowH);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (0, rowY, kW, 1);
    g.fillRect (0, rowY + rowH - 1, kW, 1);
    // Mode row sections: three glass wells, hairline dividers in the gaps between them.
    for (auto w : { scaleWell, stabilizerWell, rangeWell })
        drawSectionPanel (g, w, 9.0f);
    for (float x : { (scaleWell.getRight() + stabilizerWell.getX()) * 0.5f, (stabilizerWell.getRight() + rangeWell.getX()) * 0.5f })
        drawHairline (g, { x, rowY + 7.0f }, { x, rowY + rowH - 7.0f }, 0.16f);
    g.setColour (textDim);
    g.setFont (font (12.0f, true));
    auto label = [&] (const char* t, juce::Rectangle<float> w, float width) {
        g.drawText (t, juce::Rectangle<float> (w.getX() + 9.0f, w.getY(), width, w.getHeight()), juce::Justification::centredLeft);
    };
    label ("Scale", scaleWell, 44.0f);
    label ("Stabilizer", stabilizerWell, 68.0f);
    label ("Range", rangeWell, 46.0f);

    // Doubler section: same well + a hairline between its title and its settings.
    const bool dblOn = e.state.getRawParameterValue (ParamID::doubler)->load() > 0.5f;
    drawSectionPanel (g, doublerFrame, 10.0f);
    drawHairline (g, { doublerFrame.getX() + 10.0f, 90.0f }, { doublerFrame.getRight() - 10.0f, 90.0f }, dblOn ? 0.22f : 0.12f);
    g.setColour (dblOn ? textDim : textFaint);
    g.setFont (font (11.5f, true));
    g.drawText ("Width", 18, 96, 96, 14, juce::Justification::centredLeft);
    g.drawText ("Blend", 18, 142, 96, 14, juce::Justification::centredLeft);

    // Latency section: readout | hairline | HQ toggle (latency-sensitive settings stay visible together).
    drawSectionPanel (g, latencyFrame, 10.0f);
    drawHairline (g, { 168.0f, latencyFrame.getY() + 8.0f }, { 168.0f, latencyFrame.getBottom() - 8.0f }, 0.16f);
    g.setFont (font (12.0f, true));
    g.setColour (textDim);
    g.drawText ("LATENCY", 18, int (latencyFrame.getY()), 60, int (latencyFrame.getHeight()), juce::Justification::centredLeft);
    g.setColour (lavender);
    g.setFont (font (13.0f, true));
    g.drawText (e.latencyText, 78, int (latencyFrame.getY()), 88, int (latencyFrame.getHeight()), juce::Justification::centredLeft);

    // Bottom left column.
    g.setColour (bg2);
    g.fillRect (0, rowY + rowH, keysX - 8, kH - rowY - rowH);
    if (e.proc.ui.heatmaps)
    {
        g.setColour (textDim);
        g.setFont (font (11.5f, true));
        g.drawText ("Input", 12, keysY + e.keyboard.heatInputY() - 2, 140, 16, juce::Justification::centredRight);
        g.drawText ("Output", 12, keysY + e.keyboard.heatOutputY() - 2, 140, 16, juce::Justification::centredRight);
    }
    const int root = juce::jlimit (0, 11, int (e.state.getRawParameterValue (ParamID::root)->load()));
    const int scale = juce::jlimit (0, 3, int (e.state.getRawParameterValue (ParamID::scale)->load()));
    g.setColour (text);
    g.setFont (font (64.0f, true));
    g.drawText (kNoteNames[root], e.rootArea.withHeight (78), juce::Justification::centred);
    g.setColour (lavender);
    g.setFont (font (15.0f, true));
    g.drawText (juce::String (kScaleNames[scale]).toLowerCase(), e.rootArea.withTrimmedTop (78).withHeight (22), juce::Justification::centred);

    // Bottom right column.
    g.setColour (bg2);
    g.fillRect (keysX + keysW + 8, rowY + rowH, kW - keysX - keysW - 8, kH - rowY - rowH);
    g.setColour (textDim);
    g.setFont (font (11.5f, true));
    g.drawText ("Pitch Reference", rightX, 530, 120, 16, juce::Justification::centred);
}

void LyraTuneEditor::timerCallback()
{
    // Drain DSP telemetry: every sample feeds the HeatMaps, the newest drives the live displays.
    const double now = juce::Time::getMillisecondCounterHiRes();
    // While bypassed nothing is corrected: show output = input and no target.
    const bool bypassed = state.getRawParameterValue (ParamID::bypass)->load() > 0.5f;
    lyra::Telemetry t;
    while (proc.telemetry.pop (t))
    {
        latestRms = t.rms;
        if (t.inputMidi > 0 && t.confidence >= 0.7f)
        {
            const float out = bypassed ? t.inputMidi : t.outputMidi;
            keyboard.addHeat (t.inputMidi, out, t.confidence * juce::jmin (1.0f, t.rms * 8.0f) * 0.35f);
            display.inputMidi = t.inputMidi;
            display.outputMidi = out;
            display.target = bypassed ? -1 : t.target;
            display.pending = bypassed ? -1 : t.pending;
            lastVoicedMs = now;
        }
    }
    keyboard.decayHeat (0.93f);
    display.voiced = now - lastVoicedMs < 80.0;
    display.presence += display.voiced ? (1.0f - display.presence) * 0.35f : -display.presence * 0.10f;
    const float lvl = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (latestRms, -100.0f) + 50.0f) / 40.0f);
    display.level += (lvl - display.level) * 0.25f;

    orb.setDisplay (display, bypassed);
    keyboard.setDisplay (display);
    keyboard.setMask (proc.currentMask(), juce::jlimit (0, 11, int (state.getRawParameterValue (ParamID::root)->load())));

    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());
    aButton.setToggleState (proc.getABSlot() == 0, juce::dontSendNotification);
    bButton.setToggleState (proc.getABSlot() == 1, juce::dontSendNotification);
    copyButton.setTooltip (proc.getABSlot() == 0 ? "Copy A to B" : "Copy B to A");

    const int range = juce::jlimit (0, 2, int (state.getRawParameterValue (ParamID::range)->load()));
    const bool hq = state.getRawParameterValue (ParamID::hq)->load() > 0.5f;
    const auto lat = juce::String (proc.latencyMsFor (range, hq), 1) + " ms added";
    const bool dblOn = state.getRawParameterValue (ParamID::doubler)->load() > 0.5f;
    orb.setDoubler (dblOn, state.getRawParameterValue (ParamID::doublerWidth)->load() / 100.0f,
                    state.getRawParameterValue (ParamID::doublerBlend)->load() / 100.0f);
    widthStrip.setAlpha (dblOn ? 1.0f : 0.45f);
    blendStrip.setAlpha (dblOn ? 1.0f : 0.45f);
    if (dblOn != lastDoublerOn) { lastDoublerOn = dblOn; canvas.repaint (layout::doublerFrame.toNearestInt().expanded (2)); }
    if (lat != latencyText) { latencyText = lat; canvas.repaint (layout::latencyFrame.toNearestInt().expanded (2)); }
    canvas.repaint (0, layout::keysY + 40, layout::keysX, kH - layout::keysY - 40); // root letter / scale name
}

void LyraTuneEditor::setParam (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void LyraTuneEditor::toggleNote (int pc)
{
    const auto mask = proc.currentMask();
    proc.undoManager.beginNewTransaction ("Edit scale");
    if (int (state.getRawParameterValue (ParamID::scale)->load()) != int (lyra::ScaleType::Custom))
    {
        // Start the custom scale from what is currently audible.
        for (int i = 0; i < 12; ++i)
            setParam (ParamID::note (i), lyra::isAllowed (mask, i) ? 1.0f : 0.0f);
        setParam (ParamID::scale, float (lyra::ScaleType::Custom));
    }
    setParam (ParamID::note (pc), lyra::isAllowed (mask, pc) ? 0.0f : 1.0f);
}

void LyraTuneEditor::setRoot (int pc)
{
    proc.undoManager.beginNewTransaction ("Set root");
    const int scale = int (state.getRawParameterValue (ParamID::scale)->load());
    if (scale == int (lyra::ScaleType::Custom))
    {
        // Moving the root of a custom scale transposes it.
        const int shift = pc - int (state.getRawParameterValue (ParamID::root)->load());
        const auto moved = lyra::transposeMask (proc.currentMask(), shift);
        for (int i = 0; i < 12; ++i)
            setParam (ParamID::note (i), lyra::isAllowed (moved, i) ? 1.0f : 0.0f);
    }
    setParam (ParamID::root, float (pc));
}

void LyraTuneEditor::toggleRootMenu()
{
    if (rootMenuOpen)
    {
        juce::PopupMenu::dismissAllActiveMenus();
        return;
    }
    // The dismissing click may or may not also reach us (platform-dependent): never reopen on it.
    if (juce::Time::getMillisecondCounterHiRes() - rootMenuClosedByAreaClickMs < 400.0)
        return;
    showRootMenu();
}

void LyraTuneEditor::showRootMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Root note");
    const int cur = int (state.getRawParameterValue (ParamID::root)->load());
    for (int i = 0; i < 12; ++i)
        m.addItem (i + 1, kNoteNames[i], true, i == cur);
    rootMenuOpen = true;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (canvas).withTargetScreenArea (
                         canvas.localAreaToGlobal (rootArea)),
                     [sp = juce::Component::SafePointer<LyraTuneEditor> (this)] (int r) {
                         if (! sp) return;
                         sp->rootMenuOpen = false;
                         const auto mouse = sp->canvas.getLocalPoint (nullptr, juce::Desktop::getMousePosition());
                         if (r == 0 && sp->rootArea.contains (mouse))
                             sp->rootMenuClosedByAreaClickMs = juce::Time::getMillisecondCounterHiRes();
                         if (r > 0) sp->setRoot (r - 1);
                     });
}

void LyraTuneEditor::showMainMenu()
{
    juce::PopupMenu m, sizes;
    for (int pct : { 75, 100, 125, 150 })
        sizes.addItem (pct, juce::String (pct) + " %", true, std::abs (proc.ui.scale * 100.0f - float (pct)) < 1.0f);
    m.addSubMenu ("Window size", sizes);
    m.addSeparator();
    m.addItem (1000, juce::String (JucePlugin_Name) + " " + JucePlugin_VersionString, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton),
                     [sp = juce::Component::SafePointer<LyraTuneEditor> (this)] (int r) { if (sp && r >= 75 && r <= 150) sp->setScale (float (r) / 100.0f); });
}

void LyraTuneEditor::setScale (float s)
{
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
}
