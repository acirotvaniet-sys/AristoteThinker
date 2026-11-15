#include "Params.h"

namespace ParamChoices
{
    const juce::StringArray& delayTimes()
    {
        static const juce::StringArray s { "1/4", "1/8", "1/8 D", "1/16", "1/4 T", "1/8 T", "1/16 T" };
        return s;
    }

    const juce::StringArray& filterTypes()
    {
        static const juce::StringArray s { "Off", "Low-pass", "High-pass", "Band-pass" };
        return s;
    }

    const juce::StringArray& eqPresets()
    {
        static const juce::StringArray s { "Flat", "Dark", "Lo-fi", "Telephone", "Air", "808 Clean" };
        return s;
    }

    const juce::StringArray& analysisBars()
    {
        static const juce::StringArray s { "1 bar", "2 bars", "4 bars", "8 bars" };
        return s;
    }

    double delayTimeInBeats (int index)
    {
        static constexpr double beats[] { 1.0, 0.5, 0.75, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
        return beats[juce::jlimit (0, 6, index)];
    }

    int analysisBarsValue (int index)
    {
        static constexpr int bars[] { 1, 2, 4, 8 };
        return bars[juce::jlimit (0, 3, index)];
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 1.0f), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (
                                                          [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; }));
    };

    auto choice = [] (const char* id, const char* name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };

    // --- Modulation ---
    layout.add (pct (ParamIDs::volDepth,    "Volume Depth", 1.0f));
    layout.add (pct (ParamIDs::filterDepth, "Filter Depth", 0.6f));
    layout.add (pct (ParamIDs::panDepth,    "Pan Depth",    0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::smoothing, 1 }, "Smoothing",
                                                       NormalisableRange<float> (0.1f, 30.0f, 0.0f, 0.4f), 1.5f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (choice (ParamIDs::analysisBars, "Analysis Length", ParamChoices::analysisBars(), 2));

    // --- FX ---
    layout.add (pct (ParamIDs::satDrive,   "Saturation", 0.0f));
    layout.add (pct (ParamIDs::distAmount, "Distortion", 0.0f));
    layout.add (pct (ParamIDs::delayMix,   "Delay Mix",  0.0f));
    layout.add (choice (ParamIDs::delayTime, "Delay Time", ParamChoices::delayTimes(), 2));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::delayFeedback, 1 }, "Delay Feedback",
                                                       NormalisableRange<float> (0.0f, 0.95f), 0.35f));
    layout.add (choice (ParamIDs::filterType, "Filter Type", ParamChoices::filterTypes(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterCutoff, 1 }, "Filter Cutoff",
                                                       NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 2000.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterRes, 1 }, "Filter Resonance",
                                                       NormalisableRange<float> (0.5f, 10.0f, 0.0f, 0.4f), 0.707f));
    layout.add (choice (ParamIDs::eqPreset, "EQ Preset", ParamChoices::eqPresets(), 0));
    layout.add (pct (ParamIDs::eqAmount, "EQ Amount", 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::outputGain, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}
