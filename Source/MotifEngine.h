#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "MusicTheory.h"

// One note of the motif. 'degree' is a scale step counted from the root of the FIRST chord
// (0 = chord root, 2 = third, 4 = fifth, 7 = root an octave up, -1 = one step below ...).
struct MotifNote
{
    int step = 0;          // start, in 1/16 steps inside the motif
    int length = 2;        // length in 1/16 steps
    int degree = 0;
    float velocity = 0.9f;
};

struct Motif
{
    int lengthSteps = 16;  // 8 = half bar, 16 = 1 bar, 32 = 2 bars
    juce::uint32 seed = 1; // drives the per-repeat variation
    std::vector<MotifNote> notes;

    juce::String toString() const;
    static bool fromString (const juce::String& text, Motif& result);
};

struct GenSettings
{
    int style = 0;
    int lengthSteps = 16;
    float density = 0.6f;
    float catchiness = 0.7f;
    int range = 9;         // melody range in scale steps
};

struct RenderSettings
{
    int key = 9;           // 0 = C ... 9 = A
    int scale = 0;
    int octave = 4;
    int progression = 0;
    int mode = 0;          // 0 = Smooth, 1 = Sequence, 2 = Call & Response
    int reps = 4;
    float variation = 0.15f;
    float gate = 0.85f;
    float swing = 0.0f;
};

// A note of the full loop (motif x repeats), in quarter notes.
struct LoopNote
{
    double startQ = 0.0;
    double lengthQ = 0.25;
    int pitch = 60;
    float velocity = 0.9f;
    int rep = 0;
    int sourceIndex = 0;   // which motif note this came from
};

namespace MotifEngine
{
    Motif generate (const GenSettings&, juce::Random&);
    void newNotes (Motif&, const GenSettings&, juce::Random&);   // same rhythm, new pitches
    void newRhythm (Motif&, const GenSettings&, juce::Random&);  // new rhythm, keeps the note order
    void mutate (Motif&, juce::Random&);                         // nudge a few notes

    std::vector<LoopNote> render (const Motif&, const RenderSettings&);

    int rawChordRootForRep (const RenderSettings&, int rep);     // 0..6
    int chordRootForRep (const RenderSettings&, int rep);        // -3..3 (closest to the tonic)
    int tonicMidi (const RenderSettings&);
    int pitchToDegree (int pitch, const RenderSettings&);        // nearest absolute scale degree
    double loopLengthQ (const Motif&, const RenderSettings&);
}
