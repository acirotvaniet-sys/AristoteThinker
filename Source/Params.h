#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Identifiants de paramètres exposés à l'hôte (automatisables).
namespace ParamIDs
{
    // Modulation (onglet Génération)
    inline constexpr const char* volDepth     = "volDepth";
    inline constexpr const char* filterDepth  = "filterDepth";
    inline constexpr const char* panDepth     = "panDepth";
    inline constexpr const char* smoothing    = "smoothing";
    inline constexpr const char* analysisBars = "analysisBars";

    // FX
    inline constexpr const char* satDrive      = "satDrive";
    inline constexpr const char* distAmount    = "distAmount";
    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* delayTime     = "delayTime";
    inline constexpr const char* delayFeedback = "delayFeedback";
    inline constexpr const char* filterType    = "filterType";
    inline constexpr const char* filterCutoff  = "filterCutoff";
    inline constexpr const char* filterRes     = "filterRes";
    inline constexpr const char* eqPreset      = "eqPreset";
    inline constexpr const char* eqAmount      = "eqAmount";
    inline constexpr const char* outputGain    = "outputGain";
}

namespace ParamChoices
{
    const juce::StringArray& delayTimes();    // 1/4, 1/8, 1/8 D, 1/16, 1/4 T, 1/8 T, 1/16 T
    const juce::StringArray& filterTypes();   // Off, Low-pass, High-pass, Band-pass
    const juce::StringArray& eqPresets();     // Flat, Dark, Lo-fi, Telephone, Air, 808 Clean
    const juce::StringArray& analysisBars();  // 1, 2, 4, 8 bars

    double delayTimeInBeats (int index);
    int    analysisBarsValue (int index);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
