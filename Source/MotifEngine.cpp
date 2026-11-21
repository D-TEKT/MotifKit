#include "MotifEngine.h"
#include <algorithm>
#include <cmath>

namespace
{
struct RNote { int step; int length; int copyOf; };
using Rhythm = std::vector<RNote>;

int weightedPick (const std::vector<float>& w, juce::Random& rng)
{
    float total = 0.0f;
    for (auto v : w) total += v;
    if (total <= 0.0f) return -1;

    float r = rng.nextFloat() * total;
    for (size_t i = 0; i < w.size(); ++i)
    {
        r -= w[i];
        if (r <= 0.0f) return (int) i;
    }
    return (int) w.size() - 1;
}

int sgn (int v) { return (v > 0) - (v < 0); }

bool isChordTone (int degreeFromChordRoot)
{
    const int m = mt::mod (degreeFromChordRoot, 7);
    return m == 0 || m == 2 || m == 4;
}

// Nearest degree to d that is a chord tone of the chord on root r.
int nearestChordTone (int d, int r, int preferDir)
{
    const int dir = preferDir != 0 ? preferDir : 1;
    for (int dist = 0; dist <= 3; ++dist)
    {
        if (isChordTone (d + dist * dir - r)) return d + dist * dir;
        if (isChordTone (d - dist * dir - r)) return d - dist * dir;
    }
    return d;
}

// Nearest degree to d with (degree - base) mod 7 == m. Prefers going down (resolution feel).
int nearestWithMod (int d, int base, int m)
{
    for (int dist = 0; dist <= 6; ++dist)
    {
        if (mt::mod (d - dist - base, 7) == m) return d - dist;
        if (mt::mod (d + dist - base, 7) == m) return d + dist;
    }
    return d;
}

//==============================================================================
std::vector<int> partition (int len, const mt::Style& st, float density, juce::Random& rng)
{
    std::vector<int> d;

    // Tresillo-type groupings (3-3-2 ...) - the backbone of most catchy dance hooks
    if (len % 8 == 0 && rng.nextFloat() < st.tresillo)
    {
        static const std::vector<std::vector<int>> cells16 { { 3, 3, 3, 3, 4 }, { 3, 3, 4, 3, 3 }, { 3, 3, 2, 3, 3, 2 } };
        static const std::vector<std::vector<int>> cells8  { { 3, 3, 2 }, { 3, 3, 2 }, { 3, 3, 1, 1 }, { 2, 3, 3 }, { 3, 1, 2, 2 } };

        if (len % 16 == 0 && rng.nextFloat() < 0.4f)
        {
            for (int b = 0; b < len; b += 16)
            {
                const auto& c = cells16[(size_t) rng.nextInt ((int) cells16.size())];
                d.insert (d.end(), c.begin(), c.end());
            }
            return d;
        }

        for (int b = 0; b < len; b += 8)
        {
            const auto& c = cells8[(size_t) rng.nextInt ((int) cells8.size())];
            d.insert (d.end(), c.begin(), c.end());
        }
        return d;
    }

    const auto& durs = mt::styleDurations();
    int t = 0;
    while (t < len)
    {
        std::vector<float> w (durs.size(), 0.0f);
        for (size_t i = 0; i < durs.size(); ++i)
        {
            const int dur = durs[i];
            if (t + dur > len) continue;

            float wi = st.durW[i];
            wi *= (dur <= 2) ? (0.5f + density) : (1.5f - density);
            if ((t + dur) % 4 == 0) wi *= 1.35f;          // land on the beat grid
            if (dur >= 6 && (t % 4) != 0) wi *= 0.3f;     // long notes start on beats
            w[i] = wi;
        }

        const int pick = weightedPick (w, rng);
        if (pick < 0) { d.push_back (len - t); break; }
        d.push_back (durs[(size_t) pick]);
        t += durs[(size_t) pick];
    }
    return d;
}

void addSegments (Rhythm& out, const std::vector<int>& durs, int offset, bool allowAnacrusis,
                  const mt::Style& st, float density, juce::Random& rng)
{
    const float restP = juce::jlimit (0.0f, 0.75f, st.restAmount * 2.0f * (1.0f - density));
    int t = offset;

    for (size_t i = 0; i < durs.size(); ++i)
    {
        bool rest;
        if (i == 0) rest = allowAnacrusis && durs[i] <= 3 && rng.nextFloat() < st.anacrusis;
        else        rest = rng.nextFloat() < restP;

        if (! rest) out.push_back ({ t, durs[i], -1 });
        t += durs[i];
    }
}

Rhythm makeRhythm (const GenSettings& gs, juce::Random& rng)
{
    const auto& st = mt::style (gs.style);
    const int total = gs.lengthSteps;
    const int minNotes = total <= 8 ? 2 : (total <= 16 ? 4 : 6);
    Rhythm best;

    for (int attempt = 0; attempt < 40; ++attempt)
    {
        Rhythm r;
        const bool repeatHalf = total >= 8 && rng.nextFloat() < 0.3f + 0.65f * gs.catchiness;
        const int unit = repeatHalf ? total / 2 : total;

        addSegments (r, partition (unit, st, gs.density, rng), 0, true, st, gs.density, rng);

        if (repeatHalf)
        {
            const int firstCount = (int) r.size();
            for (int i = 0; i < firstCount; ++i)
            {
                RNote c = r[(size_t) i];
                c.step += unit;
                c.copyOf = i;
                r.push_back (c);
            }

            // A' : change the tail of the second half so it answers the first half
            if (rng.nextFloat() < 0.85f - 0.5f * gs.catchiness)
            {
                const int tail = unit >= 16 ? 8 : (unit >= 8 ? 4 : 2);
                const int tailStart = total - tail;

                r.erase (std::remove_if (r.begin(), r.end(), [tailStart] (const RNote& n) { return n.step >= tailStart; }), r.end());
                for (auto& n : r)
                    if (n.step + n.length > tailStart)
                        n.length = juce::jmax (1, tailStart - n.step);

                addSegments (r, partition (tail, st, gs.density, rng), tailStart, false, st, gs.density, rng);
            }
        }

        if ((int) r.size() >= minNotes) return r;
        if (r.size() > best.size()) best = r;
    }

    if (best.empty()) best.push_back ({ 0, 2, -1 });
    return best;
}

std::vector<int> makePitches (const Rhythm& r, const GenSettings& gs, juce::Random& rng, const std::vector<int>* keep)
{
    const auto& st = mt::style (gs.style);
    const int lo = -2;
    const int hi = lo + juce::jlimit (5, 14, gs.range);
    const float centre = 0.5f * (float) (lo + hi) - 1.0f;
    const float halfSpan = 0.5f * (float) (hi - lo) + 1.0f;

    std::vector<int> p (r.size(), 0);
    int prev = 0, prevIv = 0;

    for (size_t i = 0; i < r.size(); ++i)
    {
        const bool last = i + 1 == r.size();

        if (keep != nullptr && i < keep->size())
        {
            p[i] = (*keep)[i];
        }
        else if (r[i].copyOf >= 0 && ! last && rng.nextFloat() < 0.45f + 0.55f * gs.catchiness)
        {
            p[i] = p[(size_t) r[i].copyOf];   // repeat the first half: this is what makes hooks stick
        }
        else if (i == 0)
        {
            const int starts[] = { 0, 2, 4, 7 };
            std::vector<float> w { 0.35f, 0.25f, 0.30f, 0.10f };
            for (size_t k = 0; k < w.size(); ++k)
                if (starts[k] > hi) w[k] = 0.0f;
            const int pick = weightedPick (w, rng);
            p[0] = pick >= 0 ? starts[pick] : 0;
        }
        else
        {
            const bool strong = (r[i].step % 4) == 0;
            const bool longNote = r[i].length >= 4;
            std::vector<float> w;

            for (int c = lo; c <= hi; ++c)
            {
                const int iv = c - prev;
                const int a = std::abs (iv);
                float wi;
                switch (a)
                {
                    case 0:  wi = (0.08f + 0.9f * st.repeatNote) * (prevIv == 0 ? 0.3f : 1.0f); break;
                    case 1:  wi = 1.0f; break;
                    case 2:  wi = 0.35f + 0.6f * st.leapiness; break;
                    case 3:  wi = 0.12f + 0.3f * st.leapiness; break;
                    case 4:  wi = 0.10f + 0.3f * st.leapiness; break;
                    case 5:  wi = 0.04f; break;
                    case 7:  wi = 0.05f + 0.12f * st.leapiness; break;
                    default: wi = (a > 7) ? 0.0f : 0.01f; break;
                }

                // after a leap, step back the other way (classic melody rule)
                if (std::abs (prevIv) >= 3 && iv != 0 && sgn (iv) == sgn (prevIv)) wi *= 0.2f;
                // keep running scales going a little
                if (std::abs (prevIv) == 1 && a == 1 && sgn (iv) == sgn (prevIv)) wi *= 1.25f;
                // strong beats, long notes and the last note want chord tones
                if (strong || longNote || last) wi *= isChordTone (c) ? 2.6f : 0.3f;
                if (last)
                {
                    if (mt::mod (c, 7) == 0) wi *= 1.8f;
                    else if (mt::mod (c, 7) == 4) wi *= 1.3f;
                }
                // stay in a comfortable register
                const float dist = std::abs ((float) c - centre) / halfSpan;
                wi *= juce::jmax (0.08f, 1.0f - 0.6f * dist * dist);

                w.push_back (wi);
            }

            const int pick = weightedPick (w, rng);
            p[i] = pick >= 0 ? lo + pick : prev;
        }

        if (i > 0) prevIv = p[i] - prev;
        prev = p[i];
    }

    // never give back one single repeated note
    if (p.size() >= 3 && std::all_of (p.begin(), p.end(), [&p] (int v) { return v == p[0]; }))
        p[p.size() / 2] = juce::jlimit (lo, hi, p[0] + 2);

    return p;
}

float velocityFor (int step, juce::Random& rng)
{
    if (step % 4 == 0) return 0.95f;
    if (step % 2 == 0) return 0.84f;
    return 0.74f + 0.06f * rng.nextFloat();
}

Rhythm rhythmFromMotif (const Motif& m)
{
    Rhythm r;
    const int half = m.lengthSteps / 2;
    for (auto& n : m.notes)
        r.push_back ({ n.step, n.length, -1 });

    for (size_t i = 0; i < r.size(); ++i)
        if (r[i].step >= half)
            for (size_t j = 0; j < i; ++j)
                if (r[j].step == r[i].step - half && r[j].length == r[i].length)
                    r[i].copyOf = (int) j;
    return r;
}

void fillMotif (Motif& m, const Rhythm& r, const std::vector<int>& p, juce::Random& rng)
{
    m.notes.clear();
    for (size_t i = 0; i < r.size(); ++i)
        m.notes.push_back ({ r[i].step, r[i].length, p[i], velocityFor (r[i].step, rng) });

    std::sort (m.notes.begin(), m.notes.end(), [] (const MotifNote& a, const MotifNote& b) { return a.step < b.step; });
}
} // namespace

//==============================================================================
juce::String Motif::toString() const
{
    juce::String s;
    s << lengthSteps << ";" << (juce::int64) seed << ";";
    for (auto& n : notes)
        s << n.step << "," << n.length << "," << n.degree << "," << juce::String (n.velocity, 3) << "|";
    return s;
}

bool Motif::fromString (const juce::String& text, Motif& result)
{
    auto parts = juce::StringArray::fromTokens (text, ";", "");
    if (parts.size() < 3) return false;

    Motif m;
    m.lengthSteps = juce::jlimit (4, 64, parts[0].getIntValue());
    m.seed = (juce::uint32) parts[1].getLargeIntValue();

    auto tokens = juce::StringArray::fromTokens (parts[2], "|", "");
    for (auto& t : tokens)
    {
        if (t.trim().isEmpty()) continue;
        auto f = juce::StringArray::fromTokens (t, ",", "");
        if (f.size() < 4) continue;

        MotifNote n;
        n.step = juce::jlimit (0, m.lengthSteps - 1, f[0].getIntValue());
        n.length = juce::jlimit (1, m.lengthSteps, f[1].getIntValue());
        n.degree = juce::jlimit (-14, 21, f[2].getIntValue());
        n.velocity = juce::jlimit (0.05f, 1.0f, f[3].getFloatValue());
        m.notes.push_back (n);
    }

    if (m.notes.empty()) return false;
    result = m;
    return true;
}

//==============================================================================
Motif MotifEngine::generate (const GenSettings& gs, juce::Random& rng)
{
    Motif m;
    m.lengthSteps = gs.lengthSteps;
    m.seed = (juce::uint32) rng.nextInt();
    const auto r = makeRhythm (gs, rng);
    const auto p = makePitches (r, gs, rng, nullptr);
    fillMotif (m, r, p, rng);
    return m;
}

void MotifEngine::newNotes (Motif& m, const GenSettings& gs, juce::Random& rng)
{
    if (m.notes.empty()) { m = generate (gs, rng); return; }
    const auto r = rhythmFromMotif (m);
    const auto p = makePitches (r, gs, rng, nullptr);
    fillMotif (m, r, p, rng);
}

void MotifEngine::newRhythm (Motif& m, const GenSettings& gs, juce::Random& rng)
{
    std::vector<int> keep;
    for (auto& n : m.notes) keep.push_back (n.degree);

    m.lengthSteps = gs.lengthSteps;
    const auto r = makeRhythm (gs, rng);
    const auto p = makePitches (r, gs, rng, &keep);
    fillMotif (m, r, p, rng);
}

void MotifEngine::mutate (Motif& m, juce::Random& rng)
{
    if (m.notes.empty()) return;
    const int changes = 1 + rng.nextInt (juce::jmin (3, (int) m.notes.size()));
    for (int k = 0; k < changes; ++k)
    {
        auto& n = m.notes[(size_t) rng.nextInt ((int) m.notes.size())];
        int delta = rng.nextBool() ? 1 : -1;
        if (rng.nextFloat() < 0.25f) delta *= 2;
        n.degree = juce::jlimit (-7, 16, n.degree + delta);
    }
    m.seed = (juce::uint32) rng.nextInt();
}

//==============================================================================
int MotifEngine::rawChordRootForRep (const RenderSettings& rs, int rep)
{
    return mt::progression (rs.progression).roots[(size_t) mt::mod (rep, 4)];
}

int MotifEngine::chordRootForRep (const RenderSettings& rs, int rep)
{
    return mt::mod (rawChordRootForRep (rs, rep) + 3, 7) - 3;
}

int MotifEngine::tonicMidi (const RenderSettings& rs)
{
    return 12 * (rs.octave + 1) + rs.key;
}

int MotifEngine::pitchToDegree (int pitch, const RenderSettings& rs)
{
    const int rel = pitch - tonicMidi (rs);
    int best = 0, bestDist = 1000;
    for (int d = -21; d <= 35; ++d)
    {
        const int dist = std::abs (mt::degreeToSemitone (d, rs.scale) - rel);
        if (dist < bestDist) { bestDist = dist; best = d; }
    }
    return best;
}

double MotifEngine::loopLengthQ (const Motif& m, const RenderSettings& rs)
{
    return juce::jmax (1, rs.reps) * m.lengthSteps * 0.25;
}

std::vector<LoopNote> MotifEngine::render (const Motif& m, const RenderSettings& rs)
{
    std::vector<LoopNote> out;
    if (m.notes.empty()) return out;

    const int reps = juce::jmax (1, rs.reps);
    const double repLenQ = m.lengthSteps * 0.25;
    const int tonic = tonicMidi (rs);
    const int r0 = chordRootForRep (rs, 0);
    const int n = (int) m.notes.size();
    std::vector<int> deg ((size_t) n, 0);

    for (int rep = 0; rep < reps; ++rep)
    {
        const int rk = chordRootForRep (rs, rep);

        for (int i = 0; i < n; ++i)
        {
            const auto& note = m.notes[(size_t) i];
            // Sequence: the whole shape moves with the chord (Smooth is handled below)
            const int d = (rs.mode == 1) ? rk + note.degree : r0 + note.degree;
            deg[(size_t) i] = d;
        }

        if (rs.mode != 1 && rep > 0)
        {
            // Smooth: move the whole motif as little as possible so its anchor notes fit the new chord,
            // then nudge any anchor that still clashes - without squashing two notes together.
            auto isAnchor = [&m, n] (int i) { const auto& nt = m.notes[(size_t) i]; return nt.step % 4 == 0 || nt.length >= 3 || i == 0 || i == n - 1; };
            int bestShift = 0; float bestScore = 1.0e9f;
            const int pref = sgn (rk - r0);
            for (int s = -3; s <= 3; ++s)
            {
                float score = (float) std::abs (s) + (sgn (s) == pref ? 0.0f : 0.1f);
                for (int i = 0; i < n; ++i)
                    if (isAnchor (i) && ! isChordTone (r0 + m.notes[(size_t) i].degree + s - rk)) score += 3.0f;
                if (score < bestScore) { bestScore = score; bestShift = s; }
            }

            for (int i = 0; i < n; ++i)
                deg[(size_t) i] = r0 + m.notes[(size_t) i].degree + bestShift;

            for (int i = 0; i < n; ++i)
            {
                if (! isAnchor (i) || isChordTone (deg[(size_t) i] - rk)) continue;
                const int origIv = i > 0 ? m.notes[(size_t) i].degree - m.notes[(size_t) i - 1].degree : 0;
                int dir = origIv != 0 ? sgn (origIv) : (pref != 0 ? pref : 1);
                int cand = deg[(size_t) i] + dir;
                if (! isChordTone (cand - rk) || (i > 0 && origIv != 0 && cand == deg[(size_t) i - 1]))
                    cand = deg[(size_t) i] - dir;
                if (isChordTone (cand - rk)) deg[(size_t) i] = cand;
            }
        }

        // Call & Response: every 2nd repeat answers; the last one resolves home
        if (rs.mode == 2 && (rep % 2 == 1 || reps == 1))
        {
            const bool finalRep = rep == reps - 1;
            const int lastIdx = n - 1;
            const int from = n >= 2 ? deg[(size_t) lastIdx - 1] : deg[(size_t) lastIdx];   // land close to the note before
            int target;
            if (finalRep) target = isChordTone (0 - rk) ? nearestWithMod (from, 0, 0)
                                                        : nearestWithMod (from, rk, 0);
            else          target = nearestWithMod (from, rk, 4);

            deg[(size_t) lastIdx] = target;
            if (n >= 2 && std::abs (deg[(size_t) lastIdx - 1] - target) > 2)
                deg[(size_t) lastIdx - 1] = target + (deg[(size_t) lastIdx - 1] > target ? 1 : -1);
        }

        // Variation: small, repeatable changes per repeat
        if (rep > 0 && rs.variation > 0.001f && n > 1)
        {
            juce::Random vr ((juce::int64) m.seed * 7919 + rep * 104729);
            const int changes = juce::roundToInt (rs.variation * (float) n * 0.5f);
            for (int c = 0; c < changes; ++c)
            {
                const int idx = 1 + vr.nextInt (n - 1);
                if (rs.mode == 2 && idx >= n - 2) continue;   // keep the cadence intact

                const int delta = vr.nextBool() ? 1 : -1;
                int nd = deg[(size_t) idx] + delta;
                if (m.notes[(size_t) idx].step % 4 == 0 && ! isChordTone (nd - rk))
                    nd = nearestChordTone (nd, rk, delta);
                deg[(size_t) idx] = nd;
            }
        }

        for (int i = 0; i < n; ++i)
        {
            const auto& note = m.notes[(size_t) i];
            LoopNote ln;
            const double swingOffset = (note.step % 2 == 1) ? rs.swing * 0.125 : 0.0;
            ln.startQ = rep * repLenQ + note.step * 0.25 + swingOffset;
            ln.lengthQ = juce::jmax (0.06, note.length * 0.25 * rs.gate);
            ln.pitch = juce::jlimit (0, 127, tonic + mt::degreeToSemitone (deg[(size_t) i], rs.scale));
            ln.velocity = note.velocity;
            ln.rep = rep;
            ln.sourceIndex = i;
            out.push_back (ln);
        }
    }

    std::sort (out.begin(), out.end(), [] (const LoopNote& a, const LoopNote& b) { return a.startQ < b.startQ; });

    // monophonic: a note never runs into the next one
    const double loopQ = reps * repLenQ;
    for (size_t k = 0; k < out.size(); ++k)
    {
        const double next = (k + 1 < out.size()) ? out[k + 1].startQ : out[0].startQ + loopQ;
        const double maxLen = next - out[k].startQ - 0.01;
        if (out[k].lengthQ > maxLen) out[k].lengthQ = juce::jmax (0.03, maxLen);
    }
    return out;
}
