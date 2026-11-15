#include "FxChain.h"
#include "Params.h"
#include <cmath>

FxChain::FxChain()
    : oversampling (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true)
{
    for (auto& c : eqCoeffs)
        c = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);

    for (auto& ch : eq)
        for (size_t b = 0; b < ch.size(); ++b)
            ch[b].coefficients = eqCoeffs[b];
}

void FxChain::prepare (double sr, int maxBlockSize, int channels)
{
    sampleRate = sr;
    numChannels = juce::jlimit (1, 2, channels);

    oversampling.initProcessing ((size_t) maxBlockSize);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, (juce::uint32) numChannels };
    delay.prepare (spec);
    delay.setMaximumDelayInSamples ((int) (sr * 4.0));
    filter.prepare (spec);

    const double osRate = sr * 2.0;
    satSm.reset (osRate, 0.05);
    distSm.reset (osRate, 0.05);
    mixSm.reset (sr, 0.05);
    fbSm.reset (sr, 0.05);
    outSm.reset (sr, 0.05);
    cutoffSm.reset (sr, 0.05);
    delaySamplesSm.reset (sr, 0.12);
    cutoffSm.setCurrentAndTargetValue (2000.0f);
    outSm.setCurrentAndTargetValue (1.0f);
    delaySamplesSm.setCurrentAndTargetValue ((float) (sr * 0.25));

    lastEqPreset = -1;
    lastFilterType = -1;
    reset();
}

void FxChain::reset()
{
    oversampling.reset();
    delay.reset();
    filter.reset();
    fbLp[0] = fbLp[1] = 0.0f;
    for (auto& ch : eq)
        for (auto& f : ch)
            f.reset();
}

int FxChain::getLatencySamples() const
{
    return juce::roundToInt (oversampling.getLatencyInSamples());
}

//==============================================================================
void FxChain::updateEq (int preset, float amount)
{
    using B = BandType;
    static const std::array<std::array<Band, 4>, 6> presets {{
        // Flat
        {{ { B::Off, 100, 0, 0.7f }, { B::Off, 1000, 0, 0.7f }, { B::Off, 5000, 0, 0.7f }, { B::Off, 10000, 0, 0.7f } }},
        // Dark
        {{ { B::LowShelf, 120, 2.0f, 0.7f }, { B::Peak, 400, 1.5f, 0.8f }, { B::HighShelf, 3000, -9.0f, 0.7f }, { B::HighCut, 9000, 0, 0.7f } }},
        // Lo-fi
        {{ { B::LowCut, 180, 0, 0.7f }, { B::Peak, 1000, 3.0f, 0.9f }, { B::HighShelf, 2500, -4.0f, 0.7f }, { B::HighCut, 4500, 0, 0.9f } }},
        // Telephone
        {{ { B::LowCut, 450, 0, 1.0f }, { B::Peak, 1500, 6.0f, 1.2f }, { B::Peak, 800, -2.0f, 1.0f }, { B::HighCut, 3200, 0, 1.0f } }},
        // Air
        {{ { B::LowShelf, 100, -1.0f, 0.7f }, { B::Peak, 3000, 1.5f, 0.8f }, { B::HighShelf, 10000, 6.0f, 0.7f }, { B::Off, 10000, 0, 0.7f } }},
        // 808 Clean
        {{ { B::LowCut, 28, 0, 0.8f }, { B::Peak, 55, 3.0f, 1.2f }, { B::Peak, 300, -4.0f, 1.0f }, { B::HighShelf, 8000, 1.0f, 0.7f } }},
    }};

    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    const auto& bands = presets[(size_t) juce::jlimit (0, 5, preset)];
    const float sr = (float) sampleRate;
    const float nyq = sr * 0.45f;

    for (size_t b = 0; b < 4; ++b)
    {
        const auto& band = bands[b];
        const float gain = juce::Decibels::decibelsToGain (band.gainDb * amount);
        eqActive[b] = band.type != B::Off && amount > 0.001f;
        if (! eqActive[b])
            continue;

        switch (band.type)
        {
            case B::LowCut:
            {
                const float f = 20.0f * std::pow (band.freq / 20.0f, amount);
                *eqCoeffs[b] = AC::makeHighPass (sampleRate, juce::jmin (f, nyq), band.q);
                break;
            }
            case B::HighCut:
            {
                const float f = 20000.0f * std::pow (band.freq / 20000.0f, amount);
                *eqCoeffs[b] = AC::makeLowPass (sampleRate, juce::jmin (f, nyq), band.q);
                break;
            }
            case B::LowShelf:  *eqCoeffs[b] = AC::makeLowShelf  (sampleRate, juce::jmin (band.freq, nyq), band.q, gain); break;
            case B::HighShelf: *eqCoeffs[b] = AC::makeHighShelf (sampleRate, juce::jmin (band.freq, nyq), band.q, gain); break;
            case B::Peak:      *eqCoeffs[b] = AC::makePeakFilter (sampleRate, juce::jmin (band.freq, nyq), band.q, gain); break;
            case B::Off:       break;
        }
    }
}

//==============================================================================
void FxChain::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (numChannels, buffer.getNumChannels());
    if (n == 0 || numCh == 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);

    // --- 1. Saturation + distorsion (sur-échantillonnage x2 contre l'aliasing) ---
    satSm.setTargetValue (s.satDrive);
    distSm.setTargetValue (s.distAmount);
    {
        auto up = oversampling.processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        float* ch[2] { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };

        for (int i = 0; i < un; ++i)
        {
            const float sd = satSm.getNextValue();
            const float dd = distSm.getNextValue();
            if (sd < 1.0e-4f && dd < 1.0e-4f)
                continue;

            const float g = 1.0f + 9.0f * sd;
            const float comp = std::pow (g, -0.6f);
            const float g2 = 1.0f + 30.0f * dd;

            for (int c = 0; c < numCh; ++c)
            {
                float x = ch[c][i];
                // saturation douce (tanh), compensée en gain
                x = (1.0f - sd) * x + sd * std::tanh (g * x) * comp;
                // distorsion : écrêtage dur + repliement sinusoïdal
                const float pre = g2 * x;
                const float d = 0.7f * juce::jlimit (-1.0f, 1.0f, pre) + 0.3f * std::sin (pre);
                ch[c][i] = (1.0f - dd) * x + dd * d * 0.6f;
            }
        }
        oversampling.processSamplesDown (block);
    }

    // --- 2. Filtre (LP / HP / BP) ---
    if (s.filterType != lastFilterType)
    {
        if (s.filterType == 1) filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        if (s.filterType == 2) filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        if (s.filterType == 3) filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.reset();
        lastFilterType = s.filterType;
    }
    if (s.filterType > 0)
    {
        filter.setResonance (s.resonance);
        cutoffSm.setTargetValue (juce::jlimit (20.0f, (float) sampleRate * 0.45f, s.cutoff));
        for (int i = 0; i < n; ++i)
        {
            if ((i & 31) == 0)
                filter.setCutoffFrequency (cutoffSm.getNextValue());
            else
                cutoffSm.getNextValue();
            for (int c = 0; c < numCh; ++c)
                buffer.getWritePointer (c)[i] = filter.processSample (c, buffer.getReadPointer (c)[i]);
        }
    }

    // --- 3. EQ à presets ---
    if (s.eqPreset != lastEqPreset || std::abs (s.eqAmount - lastEqAmount) > 1.0e-3f)
    {
        updateEq (s.eqPreset, s.eqAmount);
        lastEqPreset = s.eqPreset;
        lastEqAmount = s.eqAmount;
    }
    for (int c = 0; c < numCh; ++c)
    {
        auto mono = block.getSingleChannelBlock ((size_t) c);
        juce::dsp::ProcessContextReplacing<float> ctx (mono);
        for (size_t b = 0; b < 4; ++b)
            if (eqActive[b])
                eq[(size_t) c][b].process (ctx);
    }

    // --- 4. Delay synchronisé au tempo ---
    const double beats = ParamChoices::delayTimeInBeats (s.delayTimeIndex);
    const float targetDelay = (float) juce::jlimit (1.0, sampleRate * 4.0 - 2.0, beats * 60.0 / juce::jmax (1.0, s.bpm) * sampleRate);
    delaySamplesSm.setTargetValue (targetDelay);
    mixSm.setTargetValue (s.delayMix);
    fbSm.setTargetValue (s.delayFeedback);

    for (int i = 0; i < n; ++i)
    {
        const float dsamp = delaySamplesSm.getNextValue();
        const float mix = mixSm.getNextValue();
        const float fb = fbSm.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            float* d = buffer.getWritePointer (c);
            const float x = d[i];
            const float wet = delay.popSample (c, dsamp);
            fbLp[c] += 0.4f * (wet - fbLp[c]);                    // répétitions qui s'assombrissent
            delay.pushSample (c, x + std::tanh (fbLp[c] * fb));
            d[i] = x * (1.0f - mix) + wet * mix;
        }
    }

    // --- 5. Gain de sortie ---
    outSm.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue();
        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[i] *= g;
    }
}
