#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Chaîne FX (onglet FX), appliquée après la modulation :
// saturation -> distorsion (sur-échantillonnées x2) -> filtre -> EQ -> delay -> sortie.
class FxChain
{
public:
    struct Settings
    {
        float satDrive = 0.0f, distAmount = 0.0f;
        float delayMix = 0.0f, delayFeedback = 0.35f;
        int delayTimeIndex = 2;
        int filterType = 0;            // 0 Off, 1 LP, 2 HP, 3 BP
        float cutoff = 2000.0f, resonance = 0.707f;
        int eqPreset = 0;
        float eqAmount = 1.0f;
        float outputDb = 0.0f;
        double bpm = 120.0;
    };

    FxChain();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings& s);

    int getLatencySamples() const;

private:
    enum class BandType { Off, LowCut, LowShelf, Peak, HighShelf, HighCut };
    struct Band { BandType type; float freq, gainDb, q; };

    void updateEq (int preset, float amount);

    double sampleRate = 44100.0;
    int numChannels = 2;

    juce::dsp::Oversampling<float> oversampling;
    juce::SmoothedValue<float> satSm, distSm, mixSm, fbSm, outSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSm;
    juce::SmoothedValue<float> delaySamplesSm;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    float fbLp[2] {};

    juce::dsp::StateVariableTPTFilter<float> filter;
    int lastFilterType = -1;

    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, 4> eqCoeffs;
    std::array<std::array<juce::dsp::IIR::Filter<float>, 4>, 2> eq;
    std::array<bool, 4> eqActive {};
    int lastEqPreset = -1;
    float lastEqAmount = -1.0f;
};
