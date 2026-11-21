#include "PluginEditor.h"

using namespace mkColours;

//==============================================================================
MotifLookAndFeel::MotifLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1d2030));
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff161823));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d2030));
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, text.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textDim);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1d2030));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, accent.withAlpha (0.5f));
}

void MotifLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float lineW = juce::jmax (3.0f, radius * 0.16f);

    juce::Path bgArc, valArc;
    bgArc.addCentredArc (c.x, c.y, radius - lineW, radius - lineW, 0.0f, startAngle, endAngle, true);
    valArc.addCentredArc (c.x, c.y, radius - lineW, radius - lineW, 0.0f, startAngle, angle, true);

    g.setColour (panelEdge);
    g.strokePath (bgArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (valArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - lineW * 2.2f;
    g.setColour (juce::Colour (0xff20243a));
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f);

    const juce::Point<float> tip (c.x + std::sin (angle) * inner * 0.8f, c.y - std::cos (angle) * inner * 0.8f);
    g.setColour (accent2);
    g.drawLine (c.x, c.y, tip.x, tip.y, 2.0f);
}

void MotifLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool primary = b.getProperties().getWithDefault ("primary", false);

    juce::Colour fill = on || primary ? accent : juce::Colour (0xff1d2030);
    if (over) fill = fill.brighter (0.15f);
    if (down) fill = fill.darker (0.2f);
    if (! b.isEnabled()) fill = fill.withAlpha (0.4f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on || primary ? accent2 : panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void MotifLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isMouseOver (true) ? accent.withAlpha (0.7f) : panelEdge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (accent);
    g.fillPath (arrow);
}

//==============================================================================
MotifKitEditor::MotifKitEditor (MotifKitProcessor& p)
    : AudioProcessorEditor (&p), proc (p), dragMidi (p), roll (p)
{
    setLookAndFeel (&lnf);

    newMotifBtn.getProperties().set ("primary", true);
    newMotifBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);

    setupButton (newMotifBtn, "Brand new motif: new rhythm and new notes");
    setupButton (newNotesBtn, "Keep the rhythm, roll new notes");
    setupButton (newRhythmBtn, "Keep the note order, roll a new rhythm");
    setupButton (mutateBtn, "Change 1-3 notes a little");
    setupButton (undoBtn, "Go back to the previous motif");
    setupButton (playBtn, "Play the loop while FL Studio is stopped");
    setupButton (saveBtn, "Save the loop as a .mid file");

    newMotifBtn.onClick  = [this] { proc.generateNew(); };
    newNotesBtn.onClick  = [this] { proc.regenerateNotes(); };
    newRhythmBtn.onClick = [this] { proc.regenerateRhythm(); };
    mutateBtn.onClick    = [this] { proc.mutateMotif(); };
    undoBtn.onClick      = [this] { proc.undo(); };

    playBtn.setClickingTogglesState (true);
    playBtn.setToggleState (proc.previewOn.load(), juce::dontSendNotification);
    playBtn.onClick = [this] { proc.previewOn.store (playBtn.getToggleState()); };

    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save MIDI",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (proc.suggestedFileName()), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.writeMidiFile (f.withFileExtension ("mid"));
                              });
    };

    dragMidi.setTooltip ("Drag me onto FL Studio's piano roll or playlist");
    addAndMakeVisible (dragMidi);
    addAndMakeVisible (roll);

    setupCombo (key, "key", "KEY");
    setupCombo (scale, "scale", "SCALE");
    setupCombo (octave, "octave", "OCTAVE");
    setupCombo (progression, "progression", "CHORDS");
    setupCombo (mode, "mode", "NOTE MODE");
    setupCombo (reps, "reps", "REPEATS");
    setupCombo (style, "style", "STYLE");
    setupCombo (length, "length", "LENGTH");
    setupCombo (playMode, "playmode", "PLAY MODE");
    setupCombo (sound, "sound", "SOUND");

    auto percent = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    setupKnob (density, "density", "DENSITY", percent);
    setupKnob (catchy, "catchy", "CATCHY", percent);
    setupKnob (range, "range", "RANGE", [] (double v) { return juce::String (juce::roundToInt (v)) + " steps"; });
    setupKnob (variation, "variation", "VARIATION", percent);
    setupKnob (gate, "gate", "GATE", percent);
    setupKnob (swing, "swing", "SWING", percent);
    setupKnob (volume, "volume", "VOLUME", [] (double v) { return juce::String (v, 1) + " dB"; });

    density.slider.setTooltip ("More notes / fewer rests (used on the next New Motif or New Rhythm)");
    catchy.slider.setTooltip ("How much the motif repeats itself inside: higher = more of a hook");
    range.slider.setTooltip ("How wide the melody may move (scale steps)");
    variation.slider.setTooltip ("Small changes on each repeat, rhythm stays the same");
    mode.box.setTooltip ("Smooth: notes move as little as possible to fit each chord. Sequence: the whole shape moves with the chord. Call & Response: every 2nd repeat answers and the last one resolves home.");
    playMode.box.setTooltip ("Hold key: the motif only plays while you hold a key, transposed to that key");
    sound.box.setTooltip ("Built-in preview sound. Off = MIDI only (route it to another synth)");

    setResizable (true, true);
    setResizeLimits (900, 560, 1800, 1100);
    setSize (1040, 640);
    startTimerHz (30);
}

MotifKitEditor::~MotifKitEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MotifKitEditor::setupButton (juce::TextButton& b, const juce::String& tip)
{
    b.setTooltip (tip);
    addAndMakeVisible (b);
}

void MotifKitEditor::setupCombo (LabeledCombo& c, const juce::String& paramId, const juce::String& textLabel)
{
    c.label.setText (textLabel, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    c.label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (c.label);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c.box.addItemList (choice->choices, 1);
    addAndMakeVisible (c.box);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c.box);
}

void MotifKitEditor::setupKnob (LabeledKnob& k, const juce::String& paramId, const juce::String& textLabel,
                                std::function<juce::String (double)> textFn)
{
    k.label.setText (textLabel, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.slider.textFromValueFunction = std::move (textFn);
    addAndMakeVisible (k.slider);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
    k.slider.updateText();
}

void MotifKitEditor::placeCombo (LabeledCombo& c, juce::Rectangle<int> r)
{
    c.label.setBounds (r.removeFromTop (16));
    c.box.setBounds (r.removeFromTop (26));
}

void MotifKitEditor::placeKnob (LabeledKnob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromTop (15));
    k.slider.setBounds (r);
}

//==============================================================================
void MotifKitEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141522), 0.0f, 0.0f,
                                             bg, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // soft glow behind the title
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.18f), 90.0f, 30.0f,
                                             accent.withAlpha (0.0f), 330.0f, 30.0f, true));
    g.fillRect (0, 0, 420, 70);

    g.setColour (accent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("MOTIFKIT", 16, 12, 170, 30, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("melody motif generator", 18, 40, 170, 14, juce::Justification::centredLeft);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (accent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, r.reduced (12, 6).removeFromTop (16), juce::Justification::centredLeft);
    };
    drawPanel (harmonyPanel, "HARMONY");
    drawPanel (motifPanel, "MOTIF");
    drawPanel (playPanel, "PLAYBACK");

    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Drag a note up/down to edit (all repeats follow)  |  right-click = delete  |  double-click = add note  |  "
                "Hold-key mode: the key you hold transposes the motif",
                hintArea, juce::Justification::centredLeft);
}

void MotifKitEditor::resized()
{
    auto r = getLocalBounds().reduced (14);

    // header
    auto header = r.removeFromTop (44);
    header.removeFromLeft (190);
    auto placeBtn = [] (juce::Component& c, juce::Rectangle<int>& row, int w, bool fromRight)
    {
        auto cell = fromRight ? row.removeFromRight (w) : row.removeFromLeft (w);
        c.setBounds (cell.reduced (3, 6));
    };
    placeBtn (newMotifBtn, header, 128, false);
    placeBtn (newNotesBtn, header, 108, false);
    placeBtn (newRhythmBtn, header, 116, false);
    placeBtn (mutateBtn, header, 88, false);
    placeBtn (undoBtn, header, 74, false);
    placeBtn (saveBtn, header, 100, true);
    placeBtn (dragMidi, header, 124, true);
    placeBtn (playBtn, header, 96, true);

    r.removeFromTop (8);
    hintArea = r.removeFromBottom (18);
    r.removeFromBottom (6);
    auto bottom = r.removeFromBottom (196);
    r.removeFromBottom (10);
    roll.setBounds (r);

    // panels
    const int gap = 10;
    const int total = bottom.getWidth() - 2 * gap;
    harmonyPanel = bottom.removeFromLeft ((int) (total * 0.37f));
    bottom.removeFromLeft (gap);
    motifPanel = bottom.removeFromLeft ((int) (total * 0.35f));
    bottom.removeFromLeft (gap);
    playPanel = bottom;

    auto inner = [] (juce::Rectangle<int> p) { return p.reduced (12, 8).withTrimmedTop (22); };

    {   // HARMONY: Key Scale Octave / Chords(wide) Mode Repeats
        auto a = inner (harmonyPanel);
        auto row1 = a.removeFromTop (a.getHeight() / 2);
        auto row2 = a;
        const int w = row1.getWidth() / 3;
        placeCombo (key, row1.removeFromLeft (w).reduced (4, 2));
        placeCombo (scale, row1.removeFromLeft (w).reduced (4, 2));
        placeCombo (octave, row1.reduced (4, 2));
        placeCombo (progression, row2.removeFromLeft (w * 2 - w / 3).reduced (4, 2));
        placeCombo (mode, row2.removeFromLeft (w).reduced (4, 2));
        placeCombo (reps, row2.reduced (4, 2));
        // the mode box gets the wide label text in its popup; keep the chords box wide
    }

    {   // MOTIF: Style Length / Density Catchy Range Variation
        auto a = inner (motifPanel);
        auto row1 = a.removeFromTop (48);
        placeCombo (style, row1.removeFromLeft (row1.getWidth() * 3 / 5).reduced (4, 2));
        placeCombo (length, row1.reduced (4, 2));
        const int w = a.getWidth() / 4;
        placeKnob (density, a.removeFromLeft (w));
        placeKnob (catchy, a.removeFromLeft (w));
        placeKnob (range, a.removeFromLeft (w));
        placeKnob (variation, a);
    }

    {   // PLAYBACK: Play mode Sound / Gate Swing Volume
        auto a = inner (playPanel);
        auto row1 = a.removeFromTop (48);
        placeCombo (playMode, row1.removeFromLeft (row1.getWidth() / 2).reduced (4, 2));
        placeCombo (sound, row1.reduced (4, 2));
        const int w = a.getWidth() / 3;
        placeKnob (gate, a.removeFromLeft (w));
        placeKnob (swing, a.removeFromLeft (w));
        placeKnob (volume, a);
    }
}

void MotifKitEditor::timerCallback()
{
    const int v = proc.uiVersion.load();
    if (v != lastUiVersion)
    {
        lastUiVersion = v;
        roll.refresh();
        undoBtn.setEnabled (proc.canUndo());
    }
    roll.setPlayhead (proc.playheadQ.load());

    if (playBtn.getToggleState() != proc.previewOn.load())
        playBtn.setToggleState (proc.previewOn.load(), juce::dontSendNotification);
}
