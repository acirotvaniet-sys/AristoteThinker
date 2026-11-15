#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "model/Preset.h"
#include <array>
#include <atomic>

// Moteur de modulation temps réel : lit les courbes du preset actif
// (précalculées en tables) en fonction de la position tempo de l'hôte et
// applique volume, filtre passe-bas et panoramique.
//
// Thread-safety : l'interface appelle setTables() (thread message) ; le thread
// audio récupère la nouvelle table en début de bloc via un tryEnter() sur un
// SpinLock, donc il ne bloque jamais.
class ModulationEngine
{
public:
    static constexpr int tableSize = 2048;

    struct Tables
    {
        std::array<std::array<float, tableSize>, numLanes> data {};
        double lengthBeats = 4.0;
    };

    struct Settings
    {
        float volDepth = 1.0f, filterDepth = 0.6f, panDepth = 0.5f, smoothingMs = 1.5f;
    };

    static void render (const Preset& preset, Tables& out);

    void prepare (double sampleRate);
    void reset();
    void setTables (const Tables& t);   // thread message

    void process (juce::AudioBuffer<float>& buffer, int numSamples,
                  double ppqStart, double bpm, const Settings& s);

    float getPhase() const noexcept               { return phase.load (std::memory_order_relaxed); }
    float getLaneValue (int lane) const noexcept  { return laneValues[(size_t) lane].load (std::memory_order_relaxed); }

private:
    juce::SpinLock pendingLock;
    Tables pending;
    bool hasPending = false;

    Tables active;
    bool hasTables = false;

    double sampleRate = 44100.0;
    float volSm = 1.0f, filtSm = 1.0f, panSm = 0.5f;
    float ic1[2] {}, ic2[2] {};
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    std::atomic<float> phase { 0.0f };
    std::array<std::atomic<float>, numLanes> laneValues;
};
