#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// Small music-theory toolbox for MotifKit: scales, chord progressions, rhythm styles.
namespace mt
{
inline int mod (int a, int b) { const int m = a % b; return m < 0 ? m + b : m; }
inline int floorDiv (int a, int b) { return (a - mod (a, b)) / b; }

//==============================================================================
struct Scale { const char* name; std::array<int, 7> steps; };

inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s {
        { "Minor",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "Major",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",       { 0, 1, 3, 5, 7, 8, 10 } },
        { "Mixolydian",     { 0, 2, 4, 5, 7, 9, 10 } },
    };
    return s;
}

inline const Scale& scale (int i)
{
    const auto& s = scales();
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, i)];
}

// Scale degree (0 = tonic, 7 = tonic an octave up, -1 = 7th below) -> semitones from tonic
inline int degreeToSemitone (int degree, int scaleIndex)
{
    return floorDiv (degree, 7) * 12 + scale (scaleIndex).steps[(size_t) mod (degree, 7)];
}

inline bool pitchClassInScale (int semitonesFromTonic, int scaleIndex)
{
    const int pc = mod (semitonesFromTonic, 12);
    for (auto s : scale (scaleIndex).steps)
        if (s == pc) return true;
    return false;
}

//==============================================================================
// Chord roots are scale degrees (0-based): 0 = 1st chord of the key, 5 = 6th chord, ...
struct Progression { const char* name; std::array<int, 4> roots; };

inline const std::vector<Progression>& progressions()
{
    static const std::vector<Progression> p {
        { "1-6-3-7  Euphoric / Epic",   { 0, 5, 2, 6 } },
        { "1-7-6-7  Hardstyle classic", { 0, 6, 5, 6 } },
        { "6-7-1-1  Rising",            { 5, 6, 0, 0 } },
        { "1-4-6-5  Dark drive",        { 0, 3, 5, 4 } },
        { "1-5-6-4  Pop anthem",        { 0, 4, 5, 3 } },
        { "6-4-1-5  Emotional",         { 5, 3, 0, 4 } },
        { "1-6-4-5  Classic",           { 0, 5, 3, 4 } },
        { "1-3-7-6  Night drive",       { 0, 2, 6, 5 } },
        { "1-4-1-5  Bouncy",            { 0, 3, 0, 4 } },
        { "1-7-1-7  Two chord",         { 0, 6, 0, 6 } },
        { "1-1-1-1  One chord",         { 0, 0, 0, 0 } },
    };
    return p;
}

inline const Progression& progression (int i)
{
    const auto& p = progressions();
    return p[(size_t) juce::jlimit (0, (int) p.size() - 1, i)];
}

//==============================================================================
inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline juce::String chordName (int keyPc, int scaleIndex, int degree)
{
    const int d = mod (degree, 7);
    const int root  = degreeToSemitone (d, scaleIndex);
    const int third = degreeToSemitone (d + 2, scaleIndex) - root;
    const int fifth = degreeToSemitone (d + 4, scaleIndex) - root;

    juce::String quality;
    if (third == 3 && fifth == 6)      quality = "dim";
    else if (third == 4 && fifth == 8) quality = "aug";
    else if (third == 3)               quality = "m";

    return noteNames()[mod (keyPc + root, 12)] + quality;
}

//==============================================================================
// Rhythm styles. durW = weights for note lengths of 1, 2, 3, 4, 6, 8 sixteenths.
struct Style
{
    const char* name;
    std::array<float, 6> durW;
    float restAmount;   // how many rests
    float tresillo;     // chance of 3-3-2 style groupings
    float anacrusis;    // chance to start just after the downbeat
    float repeatNote;   // chance of repeating the same pitch
    float leapiness;    // how jumpy the melody is
};

inline const std::array<int, 6>& styleDurations()
{
    static const std::array<int, 6> d { 1, 2, 3, 4, 6, 8 };
    return d;
}

inline const std::vector<Style>& styles()
{
    static const std::vector<Style> s {
        { "Hardstyle / Euphoric", { 0.10f, 0.40f, 0.30f, 0.15f, 0.05f, 0.00f }, 0.25f, 0.55f, 0.15f, 0.15f, 0.50f },
        { "Rawstyle / Hard",      { 0.30f, 0.35f, 0.25f, 0.10f, 0.00f, 0.00f }, 0.40f, 0.45f, 0.20f, 0.35f, 0.40f },
        { "Trance",               { 0.40f, 0.40f, 0.15f, 0.05f, 0.00f, 0.00f }, 0.15f, 0.30f, 0.10f, 0.20f, 0.40f },
        { "House",                { 0.10f, 0.35f, 0.30f, 0.20f, 0.05f, 0.00f }, 0.45f, 0.35f, 0.40f, 0.30f, 0.35f },
        { "House Rap / Hip-Hop",  { 0.20f, 0.30f, 0.20f, 0.25f, 0.05f, 0.00f }, 0.50f, 0.30f, 0.35f, 0.35f, 0.30f },
        { "Pop Hook",             { 0.05f, 0.40f, 0.15f, 0.30f, 0.05f, 0.05f }, 0.30f, 0.25f, 0.30f, 0.25f, 0.45f },
        { "Afro / Reggaeton",     { 0.10f, 0.30f, 0.45f, 0.15f, 0.00f, 0.00f }, 0.30f, 0.80f, 0.20f, 0.25f, 0.35f },
        { "Cinematic / Epic",     { 0.00f, 0.20f, 0.15f, 0.35f, 0.15f, 0.15f }, 0.15f, 0.15f, 0.20f, 0.10f, 0.60f },
    };
    return s;
}

inline const Style& style (int i)
{
    const auto& s = styles();
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, i)];
}
} // namespace mt
