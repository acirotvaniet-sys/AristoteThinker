#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "model/Preset.h"

class AristoteProcessor;

// Éditeur de courbes sur grille rythmique.
//  - clic dans le vide : ajoute un point (et le déplace)
//  - glisser un point : le déplace (aimanté à la grille ; Shift = libre)
//  - clic droit sur un point : le supprime
//  - Alt/Ctrl + glisser sur un segment : courbure ; double-clic : courbure à zéro
class CurveEditor : public juce::Component, private juce::Timer
{
public:
    explicit CurveEditor (AristoteProcessor& p);

    void setLane (int laneIndex);
    int getLane() const { return lane; }

    void setGridIndex (int index);   // 0: 1/8, 1: 1/16, 2: 1/32, 3: 1/8T, 4: 1/16T
    static juce::StringArray gridNames();

    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::Rectangle<float> plot() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const;
    int hitPoint (juce::Point<float> screenPos) const;
    int segmentAt (float x) const;
    double gridStep() const;
    float snapX (float x) const;
    void commit();

    AristoteProcessor& proc;
    Preset preset;
    int lane = laneVolume;
    int grid = 1;

    int dragIndex = -1, hoverIndex = -1;
    int curveSegment = -1;
    float curveStart = 0.0f, dragStartY = 0.0f;
    bool freeMove = false;
};
