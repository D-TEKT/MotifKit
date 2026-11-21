#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// Tiny built-in preview synth so you hear the motif right away.
// 1 = Pluck, 2 = Lead (3 detuned saws), 3 = Soft (sine-ish)
struct MotifSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class MotifVoice : public juce::SynthesiserVoice
{
public:
    explicit MotifVoice (std::atomic<int>& toneRef) : tone (toneRef) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<MotifSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        const double sr = getSampleRate();
        const double freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        currentTone = juce::jlimit (1, 3, tone.load());
        level = 0.22f * (0.45f + 0.55f * velocity);

        static const float detune[3] = { 1.0f, 1.0059f, 0.9941f };
        for (int i = 0; i < 3; ++i)
        {
            phase[i] = random.nextFloat();
            inc[i] = (float) (freq * detune[i] / sr);
        }

        juce::ADSR::Parameters p;
        switch (currentTone)
        {
            case 1:  p = { 0.001f, 0.30f, 0.0f, 0.12f }; break;
            case 2:  p = { 0.004f, 0.15f, 0.75f, 0.18f }; break;
            default: p = { 0.012f, 0.30f, 0.60f, 0.25f }; break;
        }
        env.setSampleRate (sr);
        env.setParameters (p);
        env.noteOn();

        filterEnv = 1.0f;
        filterDecay = (float) std::exp (-1.0 / (0.16 * sr));
        lp1 = lp2 = 0.0f;
        coeff = computeCoeff ((float) sr);
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) env.noteOff();
        else { env.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const float sr = (float) getSampleRate();
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        for (int s = 0; s < num; ++s)
        {
            float osc = 0.0f;
            if (currentTone == 3)
            {
                const float p = phase[0];
                osc = 0.8f * std::sin (twoPi * p) + 0.2f * std::sin (2.0f * twoPi * p);
                advance (0);
            }
            else
            {
                for (int i = 0; i < 3; ++i) { osc += saw (i); advance (i); }
                osc *= 0.45f;
            }

            filterEnv *= filterDecay;
            if ((s & 15) == 0) coeff = computeCoeff (sr);
            lp1 += coeff * (osc - lp1);
            lp2 += coeff * (lp1 - lp2);

            const float v = lp2 * env.getNextSample() * level;
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.addSample (ch, start + s, v);

            if (! env.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    float computeCoeff (float sr) const
    {
        float cutoff;
        switch (currentTone)
        {
            case 1:  cutoff = 350.0f + 7500.0f * filterEnv * filterEnv; break;
            case 2:  cutoff = 5200.0f; break;
            default: cutoff = 2600.0f; break;
        }
        cutoff = juce::jmin (cutoff, sr * 0.45f);
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / sr);
    }

    static float polyBlep (float t, float dt)
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float saw (int i) const { return 2.0f * phase[i] - 1.0f - polyBlep (phase[i], inc[i]); }
    void advance (int i) { phase[i] += inc[i]; if (phase[i] >= 1.0f) phase[i] -= 1.0f; }

    std::atomic<int>& tone;
    int currentTone = 1;
    juce::ADSR env;
    juce::Random random;
    float phase[3] {}, inc[3] {};
    float level = 0.2f, filterEnv = 1.0f, filterDecay = 0.999f, lp1 = 0.0f, lp2 = 0.0f, coeff = 0.5f;
};
