#pragma once

#include "model/Preset.h"
#include "analysis/ChordAnalyzer.h"

// Couche LLM optionnelle : demande à Claude (API Anthropic) de renommer les
// presets générés par les règles et d'écrire une note de « producteur ».
// Appel HTTP bloquant : à n'utiliser QUE depuis un thread de fond.
class ClaudeAdvisor
{
public:
    struct Suggestion
    {
        juce::String presetId, name, note;
    };

    static juce::Result request (const juce::String& apiKey,
                                 const AnalysisResult& analysis,
                                 const std::vector<Preset>& presets,
                                 std::vector<Suggestion>& out);
};
