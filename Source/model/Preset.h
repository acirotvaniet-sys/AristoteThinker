#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

// Humeurs d'Aristote. Neutral et Focused sont des états de l'interface ;
// les presets portent l'une des quatre humeurs « musicales ».
enum class Mood { Neutral = 0, Focused, Pensive, Angry, Serene, Playful };

juce::String moodToString (Mood m);
Mood moodFromString (const juce::String& s);

enum LaneIndex { laneVolume = 0, laneFilter = 1, lanePan = 2, numLanes = 3 };
juce::String laneName (int lane);
float laneDefaultValue (int lane);

// Un point d'une courbe de modulation.
//  x     : position dans le motif, 0..1
//  y     : valeur, 0..1 (volume : 1 = plein, filtre : 1 = ouvert, pan : 0.5 = centre)
//  curve : courbure du segment qui PART de ce point, -1..1 (0 = linéaire)
struct CurvePoint
{
    float x = 0.0f, y = 1.0f, curve = 0.0f;
};

struct Lane
{
    std::vector<CurvePoint> points;

    float evaluate (float x) const;          // x dans [0, 1)
    void sanitize (float defaultValue);      // tri, bornes, point en 0 et en 1

    static float shape (float t, float curve);
};

struct Preset
{
    juce::String id;            // identifiant unique (Uuid)
    juce::String name;
    juce::String note;          // raisonnement « producteur »
    juce::String archetype;     // nom du motif trap utilisé par le générateur
    Mood mood = Mood::Pensive;
    double lengthBeats = 4.0;   // longueur du motif (4 = 1 mesure en 4/4)
    std::array<Lane, numLanes> lanes;
    juce::uint32 seed = 0;
    bool saved = false;         // sauvegardé = conservé lors des nouvelles générations

    juce::ValueTree toValueTree() const;
    static Preset fromValueTree (const juce::ValueTree& v);

    static Preset makeFlat();
};
