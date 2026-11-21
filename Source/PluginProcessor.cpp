#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
const char* const renderParamIds[] = { "key", "scale", "octave", "progression", "mode", "reps", "variation", "gate", "swing", "length" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MotifKitProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto choice = [&p] (const char* id, const char* name, const StringArray& items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto flt = [&p] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def));
    };

    StringArray scaleNames, progNames, styleNames;
    for (auto& s : mt::scales())       scaleNames.add (s.name);
    for (auto& s : mt::progressions()) progNames.add (s.name);
    for (auto& s : mt::styles())       styleNames.add (s.name);

    choice ("key", "Key", mt::noteNames(), 9);
    choice ("scale", "Scale", scaleNames, 0);
    choice ("octave", "Octave", StringArray { "1", "2", "3", "4", "5", "6" }, 3);
    choice ("progression", "Chords", progNames, 0);
    choice ("mode", "Note Mode", StringArray { "Smooth (follow chords)", "Sequence (shift with chords)", "Call & Response" }, 0);
    choice ("reps", "Repeats", StringArray { "2x", "4x", "8x" }, 1);

    choice ("style", "Style", styleNames, 0);
    choice ("length", "Motif Length", StringArray { "1/2 bar", "1 bar", "2 bars" }, 1);
    flt ("density", "Density", 0.0f, 1.0f, 0.6f);
    flt ("catchy", "Catchiness", 0.0f, 1.0f, 0.7f);
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "range", 1 }, "Range", 5, 14, 9));
    flt ("variation", "Variation", 0.0f, 1.0f, 0.15f);

    choice ("playmode", "Play Mode", StringArray { "Sync to song", "Hold key (transpose)" }, 0);
    choice ("sound", "Sound", StringArray { "Off (MIDI only)", "Pluck", "Lead", "Soft" }, 1);
    flt ("gate", "Gate", 0.1f, 1.0f, 0.85f);
    flt ("swing", "Swing", 0.0f, 1.0f, 0.0f);
    flt ("volume", "Volume", -36.0f, 6.0f, -6.0f);

    return { p.begin(), p.end() };
}

//==============================================================================
MotifKitProcessor::MotifKitProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MotifKitState", createLayout()),
      random (juce::Time::currentTimeMillis())
{
    for (auto* id : renderParamIds)
        apvts.addParameterListener (id, this);

    pPlayMode = apvts.getRawParameterValue ("playmode");
    pSound    = apvts.getRawParameterValue ("sound");
    pVolume   = apvts.getRawParameterValue ("volume");

    activePitch.fill (-1);
    lastOnQ.fill (-1.0e9);

    for (int i = 0; i < 6; ++i)
        synth.addVoice (new MotifVoice (tone));
    synth.addSound (new MotifSound());

    motif = MotifEngine::generate (getGenSettings(), random);
    rebuildLoop();
}

MotifKitProcessor::~MotifKitProcessor()
{
    cancelPendingUpdate();
    for (auto* id : renderParamIds)
        apvts.removeParameterListener (id, this);
}

void MotifKitProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    synth.setCurrentPlaybackSampleRate (sampleRateHz);
    outMidi.ensureSize (8192);
    activePitch.fill (-1);
    internalRunning = false;
    expectedQ = -1.0e9;
    lastGain = juce::Decibels::decibelsToGain (pVolume->load());
}

bool MotifKitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
int MotifKitProcessor::choiceIndex (const char* id) const
{
    return juce::roundToInt (apvts.getRawParameterValue (id)->load());
}

float MotifKitProcessor::value (const char* id) const
{
    return apvts.getRawParameterValue (id)->load();
}

GenSettings MotifKitProcessor::getGenSettings() const
{
    static const int lens[] = { 8, 16, 32 };
    GenSettings g;
    g.style = choiceIndex ("style");
    g.lengthSteps = lens[juce::jlimit (0, 2, choiceIndex ("length"))];
    g.density = value ("density");
    g.catchiness = value ("catchy");
    g.range = juce::roundToInt (value ("range"));
    return g;
}

RenderSettings MotifKitProcessor::getRenderSettings() const
{
    static const int repCounts[] = { 2, 4, 8 };
    RenderSettings r;
    r.key = juce::jlimit (0, 11, choiceIndex ("key"));
    r.scale = choiceIndex ("scale");
    r.octave = choiceIndex ("octave") + 1;
    r.progression = choiceIndex ("progression");
    r.mode = choiceIndex ("mode");
    r.reps = repCounts[juce::jlimit (0, 2, choiceIndex ("reps"))];
    r.variation = value ("variation");
    r.gate = value ("gate");
    r.swing = value ("swing");
    return r;
}

//==============================================================================
void MotifKitProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();   // re-render on the message thread
}

void MotifKitProcessor::handleAsyncUpdate()
{
    const auto gs = getGenSettings();
    if (gs.lengthSteps != motif.lengthSteps)
    {
        pushUndo();
        MotifEngine::newRhythm (motif, gs, random);
    }
    rebuildLoop();
}

void MotifKitProcessor::rebuildLoop()
{
    const auto rs = getRenderSettings();
    loopNotes = MotifEngine::render (motif, rs);
    loopLengthQ = MotifEngine::loopLengthQ (motif, rs);

    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        pendingPattern.count = (int) std::min<size_t> (loopNotes.size(), (size_t) maxNotes);
        pendingPattern.loopLengthQ = loopLengthQ;
        pendingPattern.tonicMidi = MotifEngine::tonicMidi (rs);
        for (int i = 0; i < pendingPattern.count; ++i)
        {
            const auto& ln = loopNotes[(size_t) i];
            pendingPattern.notes[(size_t) i] = { ln.startQ, ln.lengthQ, ln.pitch, ln.velocity };
        }
    }

    pendingVersion.fetch_add (1);
    uiVersion.fetch_add (1);
}

void MotifKitProcessor::pushUndo()
{
    undoStack.push_back (motif);
    if (undoStack.size() > 50) undoStack.erase (undoStack.begin());
}

void MotifKitProcessor::setMotif (const Motif& m)
{
    motif = m;
    rebuildLoop();
}

void MotifKitProcessor::generateNew()      { pushUndo(); motif = MotifEngine::generate (getGenSettings(), random); rebuildLoop(); }
void MotifKitProcessor::regenerateNotes()  { pushUndo(); MotifEngine::newNotes (motif, getGenSettings(), random); rebuildLoop(); }
void MotifKitProcessor::regenerateRhythm() { pushUndo(); MotifEngine::newRhythm (motif, getGenSettings(), random); rebuildLoop(); }
void MotifKitProcessor::mutateMotif()      { pushUndo(); MotifEngine::mutate (motif, random); rebuildLoop(); }

void MotifKitProcessor::undo()
{
    if (undoStack.empty()) return;
    motif = undoStack.back();
    undoStack.pop_back();

    // keep the Length menu in step with the restored motif
    const int idx = motif.lengthSteps <= 8 ? 0 : (motif.lengthSteps <= 16 ? 1 : 2);
    if (auto* p = apvts.getParameter ("length"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));

    rebuildLoop();
}

//==============================================================================
juce::String MotifKitProcessor::suggestedFileName() const
{
    const auto rs = getRenderSettings();
    juce::String scaleShort = mt::scale (rs.scale).name;
    scaleShort = scaleShort.removeCharacters (" ");
    return "MotifKit " + mt::noteNames()[rs.key] + " " + scaleShort + " "
         + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".mid";
}

bool MotifKitProcessor::writeMidiFile (const juce::File& file) const
{
    constexpr int tpq = 960;
    juce::MidiMessageSequence seq;

    auto name = juce::MidiMessage::textMetaEvent (3, "MotifKit");
    name.setTimeStamp (0.0);
    seq.addEvent (name);

    for (const auto& n : loopNotes)
    {
        auto on = juce::MidiMessage::noteOn (1, n.pitch, n.velocity);
        on.setTimeStamp (n.startQ * tpq);
        auto off = juce::MidiMessage::noteOff (1, n.pitch);
        off.setTimeStamp ((n.startQ + n.lengthQ) * tpq);
        seq.addEvent (on);
        seq.addEvent (off);
    }

    auto end = juce::MidiMessage::endOfTrack();
    end.setTimeStamp (loopLengthQ * tpq);
    seq.addEvent (end);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (seq);

    file.deleteFile();
    juce::FileOutputStream os (file);
    if (! os.openedOk()) return false;
    return mf.writeTo (os);
}

juce::File MotifKitProcessor::writeTempMidiFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MotifKit");
    dir.createDirectory();
    auto f = dir.getChildFile (suggestedFileName());
    writeMidiFile (f);
    return f;
}

//==============================================================================
void MotifKitProcessor::allNotesOff (int offset)
{
    for (auto& p : activePitch)
    {
        if (p >= 0)
        {
            outMidi.addEvent (juce::MidiMessage::noteOff (1, p), offset);
            p = -1;
        }
    }
}

void MotifKitProcessor::pushHeld (int note)
{
    removeHeld (note);
    if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;
}

void MotifKitProcessor::removeHeld (int note)
{
    for (int i = 0; i < heldCount; ++i)
    {
        if (held[(size_t) i] == note)
        {
            for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
            --heldCount;
            return;
        }
    }
}

void MotifKitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    outMidi.clear();
    if (numSamples <= 0) return;

    // 1. pick up a new pattern from the UI (never blocks)
    bool patternChanged = false;
    const int pv = pendingVersion.load();
    if (pv != audioVersion)
    {
        const juce::SpinLock::ScopedTryLockType sl (patternLock);
        if (sl.isLocked())
        {
            audioPattern = pendingPattern;
            audioVersion = pv;
            patternChanged = true;
        }
    }

    // 2. incoming keys (used by Hold-key mode)
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isNoteOn())                               pushHeld (m.getNoteNumber());
        else if (m.isNoteOff())                         removeHeld (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) heldCount = 0;
    }
    midi.clear();

    // 3. timing
    bool hostPlaying = false;
    double hostQ = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) hostQ = *q;
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    const double qPerSample = bpm / (60.0 * sampleRateHz);

    const bool holdMode = juce::roundToInt (pPlayMode->load()) == 1;
    const bool keyDown = heldCount > 0;

    bool running = false;
    double startQ = 0.0;
    if (hostPlaying)
    {
        internalRunning = false;
        running = ! holdMode || keyDown;
        startQ = hostQ;
    }
    else
    {
        const bool wantInternal = holdMode ? keyDown : previewOn.load();
        if (wantInternal)
        {
            if (! internalRunning) { internalRunning = true; internalQ = 0.0; }
            running = true;
            startQ = internalQ;
            internalQ += numSamples * qPerSample;
        }
        else
        {
            internalRunning = false;
        }
    }

    const double endQ = startQ + numSamples * qPerSample;

    if (patternChanged || ! running || std::abs (startQ - expectedQ) > 0.02)
    {
        allNotesOff (0);
        lastOnQ.fill (-1.0e9);
    }

    // 4. schedule the loop notes that fall inside this block
    const double L = audioPattern.loopLengthQ;
    if (running && L > 0.0)
    {
        int numEv = 0;
        const int maxEv = (int) events.size();
        auto firstAtOrAfter = [L, startQ] (double t) { return t + std::ceil ((startQ - t) / L - 1.0e-9) * L; };
        auto toOffset = [startQ, qPerSample, numSamples] (double t)
        {
            return juce::jlimit (0, numSamples - 1, (int) ((t - startQ) / qPerSample));
        };

        for (int i = 0; i < audioPattern.count; ++i)
        {
            const auto& pn = audioPattern.notes[(size_t) i];
            for (double t = firstAtOrAfter (pn.startQ); t < endQ && numEv < maxEv; t += L)
                events[(size_t) numEv++] = { toOffset (t), 1, i, t };
            for (double t = firstAtOrAfter (pn.startQ + pn.lengthQ); t < endQ && numEv < maxEv; t += L)
                events[(size_t) numEv++] = { toOffset (t), 0, i, t };
        }

        std::sort (events.begin(), events.begin() + numEv, [] (const Ev& a, const Ev& b)
        {
            return a.offset != b.offset ? a.offset < b.offset : a.type < b.type;
        });

        const int transpose = (holdMode && keyDown) ? held[(size_t) heldCount - 1] - audioPattern.tonicMidi : 0;

        for (int e = 0; e < numEv; ++e)
        {
            const auto& ev = events[(size_t) e];
            auto& active = activePitch[(size_t) ev.idx];

            if (ev.type == 0)
            {
                if (active >= 0) { outMidi.addEvent (juce::MidiMessage::noteOff (1, active), ev.offset); active = -1; }
                continue;
            }

            if (std::abs (ev.q - lastOnQ[(size_t) ev.idx]) < 1.0e-6) continue;   // already triggered
            lastOnQ[(size_t) ev.idx] = ev.q;

            if (active >= 0) { outMidi.addEvent (juce::MidiMessage::noteOff (1, active), ev.offset); active = -1; }

            const auto& pn = audioPattern.notes[(size_t) ev.idx];
            const int pitch = juce::jlimit (0, 127, pn.pitch + transpose);

            for (int j = 0; j < audioPattern.count; ++j)
            {
                if (activePitch[(size_t) j] == pitch)
                {
                    outMidi.addEvent (juce::MidiMessage::noteOff (1, pitch), ev.offset);
                    activePitch[(size_t) j] = -1;
                }
            }

            outMidi.addEvent (juce::MidiMessage::noteOn (1, pitch, juce::jlimit (0.05f, 1.0f, pn.velocity)), ev.offset);
            active = pitch;
        }

        double ph = std::fmod (startQ, L);
        if (ph < 0.0) ph += L;
        playheadQ.store (ph);
        expectedQ = endQ;
    }
    else
    {
        playheadQ.store (-1.0);
        expectedQ = -1.0e9;
    }

    // 5. built-in sound
    const int newTone = juce::roundToInt (pSound->load());
    tone.store (newTone);
    if (newTone != lastTone && newTone == 0) synth.allNotesOff (0, false);
    lastTone = newTone;

    if (newTone > 0)
        synth.renderNextBlock (buffer, outMidi, 0, numSamples);

    const float targetGain = juce::Decibels::decibelsToGain (pVolume->load());
    buffer.applyGainRamp (0, numSamples, lastGain, targetGain);
    lastGain = targetGain;

    // 6. MIDI out (FL: Wrapper settings > MIDI > Output port)
    midi.swapWith (outMidi);
}

//==============================================================================
void MotifKitProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("motif", motif.toString(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void MotifKitProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid() || ! tree.hasType (apvts.state.getType())) return;

        Motif m;
        const bool hasMotif = Motif::fromString (tree.getProperty ("motif").toString(), m);
        tree.removeProperty ("motif", nullptr);
        apvts.replaceState (tree);

        if (hasMotif) motif = m;
        undoStack.clear();

        if (juce::MessageManager::existsAndIsCurrentThread())
        {
            cancelPendingUpdate();
            rebuildLoop();
        }
        else
        {
            triggerAsyncUpdate();
        }
    }
}

juce::AudioProcessorEditor* MotifKitProcessor::createEditor()
{
    return new MotifKitEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MotifKitProcessor();
}
