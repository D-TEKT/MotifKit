#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>

namespace mkColours
{
    const juce::Colour bg        { 0xff0c0d13 };
    const juce::Colour panel     { 0xff151723 };
    const juce::Colour panelEdge { 0xff262a3d };
    const juce::Colour accent    { 0xffff6a2b };
    const juce::Colour accent2   { 0xffffb347 };
    const juce::Colour text      { 0xffe8e9f0 };
    const juce::Colour textDim   { 0xff8a8fa8 };

    inline juce::Colour repColour (int rep)
    {
        static const juce::Colour c[] = { juce::Colour (0xffff6a2b), juce::Colour (0xffffa53d),
                                          juce::Colour (0xffff4d6d), juce::Colour (0xffc56cf0) };
        return c[rep % 4];
    }
}

//==============================================================================
// Shows the whole loop. Drag a note up/down to edit the motif (every repeat follows),
// right-click deletes, double-click adds a note.
class PianoRollView : public juce::Component
{
public:
    explicit PianoRollView (MotifKitProcessor& p) : proc (p) { refresh(); }

    void refresh()
    {
        loop = proc.getLoopNotes();
        rs = proc.getRenderSettings();
        loopQ = juce::jmax (0.25, proc.getLoopLengthQ());
        repLenQ = proc.getMotif().lengthSteps * 0.25;

        int lo = 127, hi = 0;
        for (auto& n : loop) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
        if (loop.empty()) { lo = 60; hi = 72; }
        lo -= 3; hi += 3;
        while (hi - lo < 18) { --lo; if (hi - lo < 18) ++hi; }

        if (dragSource >= 0) { lowPitch = juce::jmin (lowPitch, lo); highPitch = juce::jmax (highPitch, hi); }
        else                 { lowPitch = lo; highPitch = hi; }

        lowPitch = juce::jlimit (0, 120, lowPitch);
        highPitch = juce::jlimit (lowPitch + 6, 127, highPitch);
        repaint();
    }

    void setPlayhead (double q)
    {
        if (std::abs (q - playQ) > 1.0e-4) { playQ = q; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace mkColours;
        const auto b = getLocalBounds().toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (b, 8.0f);

        const auto area = gridArea();
        const float rowH = rowHeight();
        const int tonic = MotifEngine::tonicMidi (rs);

        // rows
        for (int p = lowPitch; p <= highPitch; ++p)
        {
            const float y = pitchToY (p);
            const bool inScale = mt::pitchClassInScale (p - tonic, rs.scale);
            const bool isTonic = mt::mod (p - tonic, 12) == 0;

            g.setColour (isTonic ? accent.withAlpha (0.10f) : (inScale ? juce::Colour (0xff1b1e2c) : juce::Colour (0xff12141d)));
            g.fillRect (area.getX(), y, area.getWidth(), rowH);

            if (inScale && rowH >= 7.0f)
            {
                g.setColour (isTonic ? accent2 : textDim);
                g.setFont (juce::FontOptions (juce::jmin (11.0f, rowH)));
                g.drawText (mt::noteNames()[p % 12] + juce::String (p / 12 - 1),
                            juce::Rectangle<float> (b.getX() + 4.0f, y, area.getX() - b.getX() - 6.0f, rowH),
                            juce::Justification::centredRight);
            }
        }

        // repeats + chord names
        const int reps = juce::roundToInt (loopQ / repLenQ);
        for (int r = 0; r < reps; ++r)
        {
            const float x0 = qToX (r * repLenQ), x1 = qToX ((r + 1) * repLenQ);
            g.setColour (repColour (r).withAlpha (r % 2 == 0 ? 0.035f : 0.06f));
            g.fillRect (x0, area.getY(), x1 - x0, area.getHeight());

            g.setColour (repColour (r));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            const auto chord = mt::chordName (rs.key, rs.scale, MotifEngine::rawChordRootForRep (rs, r));
            g.drawText (chord, juce::Rectangle<float> (x0 + 6.0f, b.getY() + 3.0f, x1 - x0 - 8.0f, 18.0f), juce::Justification::centredLeft);
            g.setColour (textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (r == 0 ? "MOTIF" : "repeat " + juce::String (r + 1),
                        juce::Rectangle<float> (x0 + 6.0f, b.getY() + 3.0f, x1 - x0 - 10.0f, 18.0f), juce::Justification::centredRight);
        }

        // grid lines
        const int steps = juce::roundToInt (loopQ * 4.0);
        for (int s = 0; s <= steps; ++s)
        {
            const float x = qToX (s * 0.25);
            const bool bar = s % 16 == 0, beat = s % 4 == 0;
            const bool repLine = std::abs (std::fmod (s * 0.25, repLenQ)) < 1.0e-6;
            g.setColour (repLine ? juce::Colours::white.withAlpha (0.28f)
                                 : bar ? juce::Colours::white.withAlpha (0.16f)
                                       : beat ? juce::Colours::white.withAlpha (0.08f) : juce::Colours::white.withAlpha (0.03f));
            g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        }

        // notes
        for (auto& n : loop)
        {
            const auto r = noteRect (n);
            auto c = repColour (n.rep);
            if (n.sourceIndex == hoverSource || n.sourceIndex == dragSource) c = c.brighter (0.5f);
            g.setColour (c.withAlpha (0.35f + 0.65f * n.velocity));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (c.brighter (0.6f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
        }

        // playhead
        if (playQ >= 0.0)
        {
            const float x = qToX (playQ);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawLine (x, area.getY(), x, area.getBottom(), 1.5f);
        }

        g.setColour (panelEdge);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int hit = noteAt (e.position);
        const int src = hit >= 0 ? loop[(size_t) hit].sourceIndex : -1;
        if (src != hoverSource) { hoverSource = src; repaint(); }
        setMouseCursor (hit >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hoverSource = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int hit = noteAt (e.position);

        if (e.mods.isPopupMenu())
        {
            if (hit >= 0)
            {
                auto m = proc.getMotif();
                const int src = loop[(size_t) hit].sourceIndex;
                if (m.notes.size() > 1 && src < (int) m.notes.size())
                {
                    proc.pushUndo();
                    m.notes.erase (m.notes.begin() + src);
                    proc.setMotif (m);
                    hoverSource = -1;
                    refresh();
                }
            }
            return;
        }

        if (hit >= 0)
        {
            dragSource = loop[(size_t) hit].sourceIndex;
            dragStartY = e.position.y;
            dragStartDegree = proc.getMotif().notes[(size_t) dragSource].degree;
            dragPushed = false;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragSource < 0) return;
        auto m = proc.getMotif();
        if (dragSource >= (int) m.notes.size()) return;

        const float degreeHeight = rowHeight() * 1.7f;
        const int delta = juce::roundToInt ((dragStartY - e.position.y) / degreeHeight);
        const int nd = juce::jlimit (-7, 16, dragStartDegree + delta);

        if (m.notes[(size_t) dragSource].degree != nd)
        {
            if (! dragPushed) { proc.pushUndo(); dragPushed = true; }
            m.notes[(size_t) dragSource].degree = nd;
            proc.setMotif (m);
            refresh();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragSource = -1;
        refresh();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (noteAt (e.position) >= 0 || ! gridArea().contains (e.position)) return;

        auto m = proc.getMotif();
        const double q = juce::jlimit (0.0, loopQ - 0.001, xToQ (e.position.x));
        const int rep = juce::jlimit (0, juce::jmax (0, rs.reps - 1), (int) (q / repLenQ));
        const int step = juce::jlimit (0, m.lengthSteps - 1, (int) ((q - rep * repLenQ) / 0.25));

        for (auto& n : m.notes)
        {
            if (n.step == step) return;
            if (n.step < step && n.step + n.length > step) n.length = step - n.step;   // split
        }

        int nextStart = m.lengthSteps;
        for (auto& n : m.notes)
            if (n.step > step) nextStart = juce::jmin (nextStart, n.step);

        const int len = juce::jmax (1, juce::jmin (2, nextStart - step));
        const int absDeg = MotifEngine::pitchToDegree (yToPitch (e.position.y), rs);
        const int base = rs.mode == 1 ? MotifEngine::chordRootForRep (rs, rep) : MotifEngine::chordRootForRep (rs, 0);

        proc.pushUndo();
        m.notes.push_back ({ step, len, juce::jlimit (-7, 16, absDeg - base), step % 4 == 0 ? 0.95f : 0.82f });
        std::sort (m.notes.begin(), m.notes.end(), [] (const MotifNote& a, const MotifNote& b) { return a.step < b.step; });
        proc.setMotif (m);
        refresh();
    }

private:
    juce::Rectangle<float> gridArea() const
    {
        return getLocalBounds().toFloat().withTrimmedTop (24.0f).withTrimmedLeft (40.0f).withTrimmedRight (6.0f).withTrimmedBottom (6.0f);
    }

    float rowHeight() const { return gridArea().getHeight() / (float) (highPitch - lowPitch + 1); }
    float pitchToY (int p) const { return gridArea().getY() + (float) (highPitch - p) * rowHeight(); }
    int yToPitch (float y) const { return juce::jlimit (0, 127, highPitch - (int) ((y - gridArea().getY()) / rowHeight())); }
    float qToX (double q) const { const auto a = gridArea(); return a.getX() + (float) (q / loopQ) * a.getWidth(); }
    double xToQ (float x) const { const auto a = gridArea(); return (double) ((x - a.getX()) / a.getWidth()) * loopQ; }

    juce::Rectangle<float> noteRect (const LoopNote& n) const
    {
        const float x0 = qToX (n.startQ), x1 = qToX (n.startQ + n.lengthQ);
        return { x0 + 0.5f, pitchToY (n.pitch) + 0.5f, juce::jmax (3.0f, x1 - x0 - 1.0f), juce::jmax (2.0f, rowHeight() - 1.0f) };
    }

    int noteAt (juce::Point<float> p) const
    {
        for (int i = (int) loop.size() - 1; i >= 0; --i)
            if (noteRect (loop[(size_t) i]).expanded (1.0f, 2.0f).contains (p))
                return i;
        return -1;
    }

    MotifKitProcessor& proc;
    std::vector<LoopNote> loop;
    RenderSettings rs;
    double loopQ = 16.0, repLenQ = 4.0, playQ = -1.0;
    int lowPitch = 55, highPitch = 80;
    int hoverSource = -1, dragSource = -1, dragStartDegree = 0;
    float dragStartY = 0.0f;
    bool dragPushed = false;
};

//==============================================================================
// Drag this onto FL Studio's piano roll / playlist / channel rack to drop the melody as MIDI.
class DragMidiComponent : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragMidiComponent (MotifKitProcessor& p) : proc (p)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (mkColours::accent.withAlpha (isMouseOver() ? 0.30f : 0.16f));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (mkColours::accent);
        g.drawRoundedRectangle (b, 6.0f, 1.2f);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x87\xa3 DRAG MIDI"), getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 5) return;
        dragging = true;

        const auto file = proc.writeTempMidiFile();
        if (! file.existsAsFile()) { dragging = false; return; }

        juce::StringArray files;
        files.add (file.getFullPathName());
        juce::Component::SafePointer<DragMidiComponent> safe (this);
        juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this, [safe]
        {
            if (safe != nullptr) safe->dragging = false;
        });
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    MotifKitProcessor& proc;
    bool dragging = false;
};
