#pragma once

#include <juce_core/juce_core.h>
#include <vector>

struct ChordEvent
{
    double beat = 0.0;        // position (en temps) depuis le début de la capture
    int root = 0;             // 0 = C ... 11 = B
    juce::String quality;     // "", "m", "dim", "sus4", "maj7", "m7", "7"
    juce::String name;        // ex. "F#m7"
    float confidence = 0.0f;
};

struct AnalysisResult
{
    bool valid = false;
    bool silent = true;

    int keyRoot = 9;          // A
    bool keyMinor = true;
    juce::String keyName = "A minor";

    std::vector<ChordEvent> chords;

    // Descripteurs normalisés 0..1 utilisés par le moteur de règles trap
    float darkness   = 0.5f;  // accords mineurs/diminués, tonalité mineure, spectre sombre
    float brightness = 0.5f;  // centroïde spectral + accords majeurs
    float energy     = 0.5f;  // niveau RMS
    float density    = 0.5f;  // nombre d'attaques par temps

    double bpm = 120.0;
    double lengthBeats = 16.0;
    int beatsPerBar = 4;

    juce::String chordProgressionText (int maxChords = 6) const;
    juce::String summary() const;
};

// Analyse hors-ligne (thread de fond) d'un enregistrement mono synchronisé au tempo.
// Méthode : chromagramme FFT synchronisé sur les temps -> gabarits d'accords,
// profils de Krumhansl pour la tonalité, RMS / flux spectral / centroïde pour
// l'énergie, la densité et la brillance.
class ChordAnalyzer
{
public:
    static AnalysisResult analyze (const float* mono, int numSamples, double sampleRate,
                                   double bpm, int beatsPerBar);

    static juce::String pitchName (int pc);
};
