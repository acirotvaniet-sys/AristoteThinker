#pragma once

#include "Knob.h"

class AristoteProcessor;

// Onglet 2 : FX, séparé de la génération.
class FxTab : public juce::Component
{
public:
    explicit FxTab (AristoteProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section { juce::String title; juce::Rectangle<int> bounds; };
    std::vector<Section> sections;

    Knob sat, dist, delayMix, delayFb, cutoff, res, eqAmount, output;
    Choice delayTime, filterType, eqPreset;
};
