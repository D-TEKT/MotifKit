#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <array>
#include <atomic>
#include "MotifEngine.h"
#include "SynthVoice.h"

class MotifKitProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::AsyncUpdater
{
public:
    MotifKitProcessor();
    ~MotifKitProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MotifKit"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    // Message-thread API (used by the editor)
    void generateNew();
    void regenerateNotes();
    void regenerateRhythm();
    void mutateMotif();
    void undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void pushUndo();
    void setMotif (const Motif& m);

    const Motif& getMotif() const { return motif; }
    const std::vector<LoopNote>& getLoopNotes() const { return loopNotes; }
    double getLoopLengthQ() const { return loopLengthQ; }
    RenderSettings getRenderSettings() const;
    GenSettings getGenSettings() const;

    bool writeMidiFile (const juce::File& file) const;
    juce::File writeTempMidiFile() const;
    juce::String suggestedFileName() const;

    std::atomic<double> playheadQ { -1.0 };
    std::atomic<bool> previewOn { false };
    std::atomic<int> uiVersion { 0 };

private:
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void rebuildLoop();
    int choiceIndex (const char* id) const;
    float value (const char* id) const;

    void allNotesOff (int offset);
    void pushHeld (int note);
    void removeHeld (int note);

    Motif motif;
    std::vector<Motif> undoStack;
    std::vector<LoopNote> loopNotes;
    double loopLengthQ = 16.0;
    juce::Random random;

    // ---- audio-thread data (fixed size, no allocations) ----
    static constexpr int maxNotes = 512;
    struct PlayNote { double startQ = 0.0, lengthQ = 0.25; int pitch = 60; float velocity = 0.9f; };
    struct AudioPattern { int count = 0; double loopLengthQ = 4.0; int tonicMidi = 69; std::array<PlayNote, maxNotes> notes {}; };

    AudioPattern pendingPattern, audioPattern;
    juce::SpinLock patternLock;
    std::atomic<int> pendingVersion { 0 };
    int audioVersion = -1;

    struct Ev { int offset; int type; int idx; double q; };   // type 0 = off, 1 = on
    std::array<Ev, maxNotes * 4> events {};
    std::array<int, maxNotes> activePitch {};
    std::array<double, maxNotes> lastOnQ {};
    std::array<int, 128> held {};
    int heldCount = 0;

    bool internalRunning = false;
    double internalQ = 0.0, expectedQ = -1.0e9;
    double sampleRateHz = 44100.0;
    float lastGain = 0.5f;
    int lastTone = 1;

    juce::MidiBuffer outMidi;
    juce::Synthesiser synth;
    std::atomic<int> tone { 1 };

    std::atomic<float>* pPlayMode = nullptr;
    std::atomic<float>* pSound = nullptr;
    std::atomic<float>* pVolume = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotifKitProcessor)
};
