#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/AristoteLookAndFeel.h"
#include "ui/GenerationTab.h"
#include "ui/FxTab.h"

class AristoteEditor : public juce::AudioProcessorEditor
{
public:
    explicit AristoteEditor (AristoteProcessor&);
    ~AristoteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showTab (int index);
    void showAiSettings();

    AristoteLookAndFeel lnf;          // doit survivre à tous les composants
    AristoteProcessor& proc;

    juce::TextButton genTabBtn { "Generation" }, fxTabBtn { "FX" }, aiBtn { "AI" };
    GenerationTab genTab;
    FxTab fxTab;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AristoteEditor)
};
