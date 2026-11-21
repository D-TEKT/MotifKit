#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PianoRollView.h"

class MotifLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MotifLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

class MotifKitEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MotifKitEditor (MotifKitProcessor&);
    ~MotifKitEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct LabeledCombo
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    struct LabeledKnob
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void setupCombo (LabeledCombo&, const juce::String& paramId, const juce::String& text);
    void setupKnob (LabeledKnob&, const juce::String& paramId, const juce::String& text,
                    std::function<juce::String (double)> textFn);
    static void placeCombo (LabeledCombo&, juce::Rectangle<int>);
    static void placeKnob (LabeledKnob&, juce::Rectangle<int>);
    void setupButton (juce::TextButton&, const juce::String& tip);

    MotifKitProcessor& proc;
    MotifLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton newMotifBtn { "NEW MOTIF" }, newNotesBtn { "NEW NOTES" }, newRhythmBtn { "NEW RHYTHM" },
                     mutateBtn { "MUTATE" }, undoBtn { "UNDO" }, playBtn { "PREVIEW" }, saveBtn { "SAVE MIDI" };
    DragMidiComponent dragMidi;
    PianoRollView roll;

    LabeledCombo key, scale, octave, progression, mode, reps, style, length, playMode, sound;
    LabeledKnob density, catchy, range, variation, gate, swing, volume;

    std::unique_ptr<juce::FileChooser> chooser;
    int lastUiVersion = -1;
    juce::Rectangle<int> harmonyPanel, motifPanel, playPanel, hintArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotifKitEditor)
};
