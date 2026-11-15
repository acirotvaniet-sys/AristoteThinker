#include "CurveEditor.h"
#include "AristoteLookAndFeel.h"
#include "PluginProcessor.h"
#include <cmath>

CurveEditor::CurveEditor (AristoteProcessor& p) : proc (p)
{
    refresh();
    startTimerHz (30);
}

juce::StringArray CurveEditor::gridNames()
{
    return { "1/8", "1/16", "1/32", "1/8 T", "1/16 T" };
}

void CurveEditor::setLane (int l)
{
    lane = juce::jlimit (0, numLanes - 1, l);
    repaint();
}

void CurveEditor::setGridIndex (int index)
{
    grid = juce::jlimit (0, 4, index);
    repaint();
}

double CurveEditor::gridStep() const
{
    static constexpr double steps[] { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    return steps[grid];
}

void CurveEditor::refresh()
{
    if (dragIndex >= 0 || curveSegment >= 0)
        return;   // ne pas écraser une édition en cours
    preset = proc.getActivePreset();
    repaint();
}

void CurveEditor::timerCallback()
{
    repaint();   // tête de lecture
}

//==============================================================================
juce::Rectangle<float> CurveEditor::plot() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 12.0f);
}

juce::Point<float> CurveEditor::toScreen (float x, float y) const
{
    const auto r = plot();
    return { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
}

juce::Point<float> CurveEditor::fromScreen (juce::Point<float> p) const
{
    const auto r = plot();
    return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()),
             juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight()) };
}

int CurveEditor::hitPoint (juce::Point<float> sp) const
{
    const auto& pts = preset.lanes[(size_t) lane].points;
    for (int i = (int) pts.size() - 1; i >= 0; --i)
        if (toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (sp) < 7.0f)
            return i;
    return -1;
}

int CurveEditor::segmentAt (float x) const
{
    const auto& pts = preset.lanes[(size_t) lane].points;
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
        if (x >= pts[(size_t) i].x && x < pts[(size_t) i + 1].x)
            return i;
    return -1;
}

float CurveEditor::snapX (float x) const
{
    if (freeMove)
        return x;
    const double len = preset.lengthBeats;
    const double step = gridStep();
    return (float) juce::jlimit (0.0, 1.0, std::round (x * len / step) * step / len);
}

void CurveEditor::commit()
{
    proc.updateActivePreset (preset);
    repaint();
}

//==============================================================================
void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    auto& pts = preset.lanes[(size_t) lane].points;
    freeMove = e.mods.isShiftDown();
    const int hit = hitPoint (e.position);

    if (e.mods.isPopupMenu())
    {
        if (hit > 0 && hit < (int) pts.size() - 1)
        {
            pts.erase (pts.begin() + hit);
            commit();
        }
        return;
    }

    if (e.mods.isAltDown() || e.mods.isCommandDown())
    {
        curveSegment = segmentAt (fromScreen (e.position).x);
        if (curveSegment >= 0)
        {
            curveStart = pts[(size_t) curveSegment].curve;
            dragStartY = e.position.y;
        }
        return;
    }

    if (hit >= 0)
    {
        dragIndex = hit;
        return;
    }

    // Ajout d'un point sur la grille
    auto v = fromScreen (e.position);
    v.x = snapX (v.x);
    if (v.x <= 0.0f || v.x >= 1.0f)
        return;
    CurvePoint np { v.x, v.y, 0.0f };
    auto it = std::upper_bound (pts.begin(), pts.end(), np.x, [] (float x, const CurvePoint& p) { return x < p.x; });
    dragIndex = (int) std::distance (pts.begin(), pts.insert (it, np));
    commit();
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    auto& pts = preset.lanes[(size_t) lane].points;
    freeMove = e.mods.isShiftDown();

    if (curveSegment >= 0 && curveSegment < (int) pts.size())
    {
        pts[(size_t) curveSegment].curve = juce::jlimit (-1.0f, 1.0f, curveStart + (e.position.y - dragStartY) / 80.0f);
        commit();
        return;
    }

    if (dragIndex < 0 || dragIndex >= (int) pts.size())
        return;

    auto v = fromScreen (e.position);
    auto& p = pts[(size_t) dragIndex];
    const bool isEdge = dragIndex == 0 || dragIndex == (int) pts.size() - 1;
    if (! isEdge)
    {
        const float lo = pts[(size_t) dragIndex - 1].x + 1.0e-4f;
        const float hi = pts[(size_t) dragIndex + 1].x - 1.0e-4f;
        p.x = juce::jlimit (lo, juce::jmax (lo, hi), snapX (v.x));
    }
    p.y = v.y;
    commit();
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
    curveSegment = -1;
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (hitPoint (e.position) >= 0)
        return;
    const int seg = segmentAt (fromScreen (e.position).x);
    if (seg >= 0)
    {
        preset.lanes[(size_t) lane].points[(size_t) seg].curve = 0.0f;
        commit();
    }
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitPoint (e.position);
    if (h != hoverIndex)
    {
        hoverIndex = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

//==============================================================================
void CurveEditor::paint (juce::Graphics& g)
{
    const auto r = plot();
    g.fillAll (Ink::paper);

    // Grille rythmique
    const double len = juce::jmax (0.25, preset.lengthBeats);
    const double step = gridStep();
    for (int k = 0; k * step < len + 1.0e-9; ++k)
    {
        const double b = k * step;
        const float x = r.getX() + (float) (b / len) * r.getWidth();
        const bool isBar = std::abs (std::fmod (b, 4.0)) < 1.0e-6;
        const bool isBeat = std::abs (b - std::round (b)) < 1.0e-6;
        g.setColour (Ink::ink.withAlpha (isBar ? 0.55f : isBeat ? 0.22f : 0.08f));
        g.drawVerticalLine ((int) std::round (x), r.getY(), r.getBottom());
    }
    for (float y : { 0.25f, 0.5f, 0.75f })
    {
        g.setColour (Ink::ink.withAlpha (y == 0.5f ? 0.12f : 0.05f));
        g.drawHorizontalLine ((int) std::round (toScreen (0, y).y), r.getX(), r.getRight());
    }

    auto curvePath = [&] (const Lane& l)
    {
        juce::Path p;
        const int n = juce::jmax (2, (int) r.getWidth());
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n;
            const auto pt = toScreen (x, l.evaluate (juce::jmin (x, 0.99999f)));
            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }
        return p;
    };

    // Courbes des autres lignes, en fantôme
    for (int l = 0; l < numLanes; ++l)
        if (l != lane)
        {
            g.setColour (Ink::ink.withAlpha (0.12f));
            g.strokePath (curvePath (preset.lanes[(size_t) l]), juce::PathStrokeType (1.0f));
        }

    // Courbe active + remplissage
    const auto& active = preset.lanes[(size_t) lane];
    auto path = curvePath (active);
    auto fill = path;
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setColour (Ink::ink.withAlpha (0.07f));
    g.fillPath (fill);
    g.setColour (Ink::ink);
    g.strokePath (path, juce::PathStrokeType (1.6f));

    // Points
    for (int i = 0; i < (int) active.points.size(); ++i)
    {
        const auto sp = toScreen (active.points[(size_t) i].x, active.points[(size_t) i].y);
        const float s = (i == hoverIndex || i == dragIndex) ? 9.0f : 6.0f;
        g.setColour (Ink::paper);
        g.fillRect (juce::Rectangle<float> (s, s).withCentre (sp));
        g.setColour (Ink::ink);
        g.drawRect (juce::Rectangle<float> (s, s).withCentre (sp), 1.2f);
    }

    // Tête de lecture
    const float ph = proc.getModulationEngine().getPhase();
    g.setColour (Ink::ink.withAlpha (0.45f));
    g.drawVerticalLine ((int) std::round (r.getX() + ph * r.getWidth()), r.getY(), r.getBottom());

    // Libellés
    g.setColour (Ink::mid);
    g.setFont (Type::caps (10.0f));
    const auto top = lane == lanePan ? "R" : lane == laneFilter ? "OPEN" : "100%";
    const auto bottom = lane == lanePan ? "L" : lane == laneFilter ? "CLOSED" : "0%";
    g.drawText (top, r.withHeight (14.0f).reduced (4.0f, 0.0f), juce::Justification::topRight);
    g.drawText (bottom, r.withTrimmedTop (r.getHeight() - 14.0f).reduced (4.0f, 0.0f), juce::Justification::bottomRight);

    g.setColour (Ink::ink);
    g.drawRect (plot(), 1.0f);
}
