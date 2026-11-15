#pragma once

#include "model/Preset.h"
#include "analysis/ChordAnalyzer.h"
#include <set>

// Moteur de règles « producteur trap » : transforme une analyse (accords,
// tonalité, énergie, densité) en presets de modulation.
class TrapGenerator
{
public:
    enum class Archetype
    {
        HalfTimePump = 0,   // pompage sidechain sur un pattern de kick/808 half-time
        TripletBounce,      // gate en triolets, ping-pong stéréo
        HiHatRoll,          // stutters façon rolls de hi-hats
        ChoppedGate,        // gate saccadé 3-3-2, coupures franches
        BreathSwell,        // respirations lentes, filtre qui s'ouvre
        numArchetypes
    };

    static juce::String archetypeName (Archetype a);
    static Archetype archetypeFromName (const juce::String& s);

    // Génère `count` presets aux archétypes distincts, choisis selon l'analyse.
    static std::vector<Preset> generate (const AnalysisResult& analysis, int count, juce::uint32 seed);

    // Génère un preset pour un archétype donné (utilisé par « Regenerate »).
    static Preset generateOne (const AnalysisResult& analysis, Archetype archetype,
                               juce::uint32 seed, std::set<juce::String>& usedNames);

    // Preset initial avant toute analyse (version minimale : un pompage half-time).
    static Preset makeDefault();
};
