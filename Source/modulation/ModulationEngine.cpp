#include "ModulationEngine.h"
#include <cmath>

void ModulationEngine::render (const Preset& preset, Tables& out)
{
    out.lengthBeats = preset.lengthBeats;
    for (size_t l = 0; l < (size_t) numLanes; ++l)
        for (int k = 0; k < tableSize; ++k)
            out.data[l][(size_t) k] = preset.lanes[l].evaluate ((float) k / (float) tableSize);
}

void ModulationEngine::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void ModulationEngine::reset()
{
    volSm = 1.0f; filtSm = 1.0f; panSm = 0.5f;
    for (int c = 0; c < 2; ++c) ic1[c] = ic2[c] = 0.0f;
    for (auto& v : laneValues) v.store (0.0f);
}

void ModulationEngine::setTables (const Tables& t)
{
    const juce::SpinLock::ScopedLockType sl (pendingLock);
    pending = t;
    hasPending = true;
}

void ModulationEngine::process (juce::AudioBuffer<float>& buffer, int numSamples,
                                double ppqStart, double bpm, const Settings& s)
{
    // Récupère une éventuelle nouvelle table sans jamais bloquer.
    if (pendingLock.tryEnter())
    {
        if (hasPending)
        {
            active = pending;
            hasPending = false;
            hasTables = true;
        }
        pendingLock.exit();
    }

    if (! hasTables || numSamples <= 0)
        return;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const double len = juce::jmax (0.25, active.lengthBeats);
    const double beatsPerSample = juce::jmax (1.0, bpm) / 60.0 / sampleRate;
    const float smooth = std::exp (-1.0f / (juce::jmax (0.05f, s.smoothingMs) * 0.001f * (float) sampleRate));
    const bool useFilter = s.filterDepth > 0.001f;
    const bool usePan = R != nullptr && s.panDepth > 0.001f;
    constexpr float k = 1.0f / 0.8f;   // Q fixe ~0.8 pour le filtre modulé
    constexpr float sqrt2 = 1.41421356f;

    auto& vol = active.data[laneVolume];
    auto& fil = active.data[laneFilter];
    auto& pan = active.data[lanePan];

    float ph = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const double ppq = ppqStart + (double) i * beatsPerSample;
        double pos = ppq / len;
        pos -= std::floor (pos);
        ph = (float) pos;

        const float idx = ph * (float) tableSize;
        const int i0 = juce::jlimit (0, tableSize - 1, (int) idx);
        const int i1 = (i0 + 1) % tableSize;
        const float fr = idx - (float) i0;

        const float vRaw = vol[(size_t) i0] + (vol[(size_t) i1] - vol[(size_t) i0]) * fr;
        const float fRaw = fil[(size_t) i0] + (fil[(size_t) i1] - fil[(size_t) i0]) * fr;
        const float pRaw = pan[(size_t) i0] + (pan[(size_t) i1] - pan[(size_t) i0]) * fr;

        volSm  = vRaw + smooth * (volSm  - vRaw);
        filtSm = fRaw + smooth * (filtSm - fRaw);
        panSm  = pRaw + smooth * (panSm  - pRaw);

        const float gain = 1.0f - s.volDepth * (1.0f - volSm);

        if (useFilter)
        {
            if ((i & 7) == 0)
            {
                // 1 = ouvert (20 kHz) ; 0 = jusqu'à 9 octaves plus bas, pondéré par la profondeur.
                const float octavesDown = (1.0f - filtSm) * s.filterDepth * 9.0f;
                const float fc = juce::jmin (20000.0f * std::exp2 (-octavesDown), 0.45f * (float) sampleRate);
                const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            for (int c = 0; c < numCh; ++c)
            {
                float* d = c == 0 ? L : R;
                const float v0 = d[i];
                const float v3 = v0 - ic2[c];
                const float v1 = a1 * ic1[c] + a2 * v3;
                const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2.0f * v1 - ic1[c];
                ic2[c] = 2.0f * v2 - ic2[c];
                d[i] = v2;   // passe-bas
            }
        }

        if (usePan)
        {
            const float p = (panSm - 0.5f) * 2.0f * s.panDepth;
            const float angle = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            L[i] *= gain * std::cos (angle) * sqrt2;
            R[i] *= gain * std::sin (angle) * sqrt2;
        }
        else
        {
            L[i] *= gain;
            if (R != nullptr) R[i] *= gain;
        }
    }

    phase.store (ph, std::memory_order_relaxed);
    laneValues[laneVolume].store (volSm, std::memory_order_relaxed);
    laneValues[laneFilter].store (filtSm, std::memory_order_relaxed);
    laneValues[lanePan].store (panSm, std::memory_order_relaxed);
}
