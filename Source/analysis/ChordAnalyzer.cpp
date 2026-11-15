#include "ChordAnalyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <numeric>

namespace
{
    constexpr int fftOrder = 12;               // 4096 points
    constexpr int fftSize  = 1 << fftOrder;
    constexpr int hopSize  = 1024;

    struct ChordTemplate
    {
        const char* quality;
        std::vector<int> intervals;
        bool minorish;
        float penalty;  // légère pénalité pour préférer les triades
    };

    const std::vector<ChordTemplate>& templates()
    {
        static const std::vector<ChordTemplate> t {
            { "",     { 0, 4, 7 },      false, 0.00f },
            { "m",    { 0, 3, 7 },      true,  0.00f },
            { "dim",  { 0, 3, 6 },      true,  0.04f },
            { "sus4", { 0, 5, 7 },      false, 0.05f },
            { "maj7", { 0, 4, 7, 11 },  false, 0.06f },
            { "m7",   { 0, 3, 7, 10 },  true,  0.05f },
            { "7",    { 0, 4, 7, 10 },  false, 0.05f },
        };
        return t;
    }

    // Profils de tonalité de Krumhansl-Kessler
    constexpr std::array<float, 12> majorProfile { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    constexpr std::array<float, 12> minorProfile { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    using Chroma = std::array<float, 12>;

    float correlation (const Chroma& a, const std::array<float, 12>& profile, int rotation)
    {
        float ma = 0.0f, mb = 0.0f;
        for (int i = 0; i < 12; ++i) { ma += a[(size_t) i]; mb += profile[(size_t) i]; }
        ma /= 12.0f; mb /= 12.0f;
        float num = 0.0f, da = 0.0f, db = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            const float x = a[(size_t) ((i + rotation) % 12)] - ma;
            const float y = profile[(size_t) i] - mb;
            num += x * y; da += x * x; db += y * y;
        }
        return num / (std::sqrt (da * db) + 1.0e-9f);
    }

    void normalise (Chroma& c)
    {
        const float m = *std::max_element (c.begin(), c.end());
        if (m > 1.0e-9f)
            for (auto& v : c) v /= m;
    }
}

juce::String ChordAnalyzer::pitchName (int pc)
{
    static const char* names[] { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[((pc % 12) + 12) % 12];
}

juce::String AnalysisResult::chordProgressionText (int maxChords) const
{
    juce::StringArray names;
    for (const auto& c : chords)
    {
        if (names.size() >= maxChords) { names.add ("..."); break; }
        names.add (c.name);
    }
    return names.isEmpty() ? juce::String ("-") : names.joinIntoString (" - ");
}

juce::String AnalysisResult::summary() const
{
    if (! valid)  return "No analysis yet";
    if (silent)   return "Silence heard - generated from defaults";
    return keyName + "  |  " + chordProgressionText()
         + "  |  energy " + juce::String (juce::roundToInt (energy * 100)) + "%";
}

//==============================================================================
AnalysisResult ChordAnalyzer::analyze (const float* mono, int numSamples, double sampleRate,
                                       double bpm, int beatsPerBar)
{
    AnalysisResult r;
    r.valid = true;
    r.bpm = bpm;
    r.beatsPerBar = juce::jmax (1, beatsPerBar);

    const double samplesPerBeat = sampleRate * 60.0 / juce::jmax (1.0, bpm);
    const int numBeats = juce::jmax (1, (int) std::floor ((double) numSamples / samplesPerBeat));
    r.lengthBeats = numBeats;

    if (mono == nullptr || numSamples < fftSize)
        return r;

    // --- Silence ? ---
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i) sumSq += (double) mono[i] * mono[i];
    const float totalRms = (float) std::sqrt (sumSq / numSamples);
    if (totalRms < 1.0e-4f)   // ~ -80 dBFS
        return r;
    r.silent = false;

    // --- Table bin FFT -> classe de hauteur ---
    const int numBins = fftSize / 2;
    std::vector<int> binPc ((size_t) numBins, -1);
    std::vector<int> binBassPc ((size_t) numBins, -1);
    std::vector<float> binFreq ((size_t) numBins);
    for (int b = 1; b < numBins; ++b)
    {
        const float f = (float) (b * sampleRate / fftSize);
        binFreq[(size_t) b] = f;
        const int midi = (int) std::lround (12.0 * std::log2 (f / 440.0) + 69.0);
        const int pc = ((midi % 12) + 12) % 12;
        if (f >= 55.0f && f <= 5000.0f) binPc[(size_t) b] = pc;
        if (f >= 40.0f && f <= 260.0f)  binBassPc[(size_t) b] = pc;
    }

    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<float> buf ((size_t) fftSize * 2);
    std::vector<float> prevMag ((size_t) numBins, 0.0f);

    std::vector<Chroma> beatChroma ((size_t) numBeats);
    std::vector<Chroma> beatBass ((size_t) numBeats);
    std::vector<float> beatRms ((size_t) numBeats, 0.0f);
    std::vector<float> flux;
    double centroidSum = 0.0, centroidWeight = 0.0;

    for (int beat = 0; beat < numBeats; ++beat)
    {
        Chroma chroma {}, bass {};
        const int start = (int) std::floor (beat * samplesPerBeat);
        const int end   = juce::jmin (numSamples, (int) std::floor ((beat + 1) * samplesPerBeat));

        double beatSq = 0.0;
        for (int i = start; i < end; ++i) beatSq += (double) mono[i] * mono[i];
        beatRms[(size_t) beat] = (float) std::sqrt (beatSq / juce::jmax (1, end - start));

        for (int pos = start; pos + fftSize <= juce::jmax (start + fftSize, end) && pos + fftSize <= numSamples; pos += hopSize)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            std::copy (mono + pos, mono + pos + fftSize, buf.begin());
            window.multiplyWithWindowingTable (buf.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (buf.data());

            float fl = 0.0f, magSum = 0.0f, magFreq = 0.0f;
            for (int b = 1; b < numBins; ++b)
            {
                const float mag = buf[(size_t) b];
                const float lm = std::log1p (mag);       // compression logarithmique
                if (binPc[(size_t) b] >= 0)     chroma[(size_t) binPc[(size_t) b]] += lm;
                if (binBassPc[(size_t) b] >= 0) bass[(size_t) binBassPc[(size_t) b]] += lm;

                fl += juce::jmax (0.0f, lm - prevMag[(size_t) b]);
                prevMag[(size_t) b] = lm;
                magSum += mag;
                magFreq += mag * binFreq[(size_t) b];
            }
            flux.push_back (fl);
            if (magSum > 1.0e-6f)
            {
                centroidSum += magFreq;          // pondéré par l'énergie de la trame
                centroidWeight += magSum;
            }

            if (end - start < fftSize)
                break; // temps très court : une seule trame
        }

        beatChroma[(size_t) beat] = chroma;
        beatBass[(size_t) beat] = bass;
    }

    // --- Accords : fenêtres de 2 temps, puis fusion des répétitions ---
    const int window2 = 2;
    float minorWeight = 0.0f, totalWeight = 0.0f;
    Chroma globalChroma {};

    for (int w = 0; w < numBeats; w += window2)
    {
        Chroma c {}, b {};
        float wRms = 0.0f;
        for (int j = w; j < juce::jmin (numBeats, w + window2); ++j)
        {
            for (int i = 0; i < 12; ++i)
            {
                c[(size_t) i] += beatChroma[(size_t) j][(size_t) i];
                b[(size_t) i] += beatBass[(size_t) j][(size_t) i];
            }
            wRms += beatRms[(size_t) j];
        }
        for (int i = 0; i < 12; ++i) globalChroma[(size_t) i] += c[(size_t) i];

        normalise (c);
        normalise (b);

        // Whitening simple : on retire la moyenne pour faire ressortir les notes jouées.
        const float mean = std::accumulate (c.begin(), c.end(), 0.0f) / 12.0f;
        Chroma cw {};
        for (int i = 0; i < 12; ++i) cw[(size_t) i] = juce::jmax (0.0f, c[(size_t) i] - mean);

        float bestScore = -1.0f;
        int bestRoot = 0;
        const ChordTemplate* best = &templates().front();
        float cwNorm = 0.0f;
        for (auto v : cw) cwNorm += v * v;
        cwNorm = std::sqrt (cwNorm) + 1.0e-9f;

        for (const auto& t : templates())
        {
            // La 7e pèse moins : les harmoniques (quinte de la tierce...) la font
            // apparaître même sur une simple triade.
            auto weightOf = [] (size_t i) { return i == 3 ? 0.6f : 1.0f; };
            float tNorm = 0.0f;
            for (size_t i = 0; i < t.intervals.size(); ++i) tNorm += weightOf (i) * weightOf (i);
            tNorm = std::sqrt (tNorm);

            for (int root = 0; root < 12; ++root)
            {
                float dot = 0.0f;
                for (size_t i = 0; i < t.intervals.size(); ++i)
                    dot += weightOf (i) * cw[(size_t) ((root + t.intervals[i]) % 12)];
                float score = dot / (cwNorm * tNorm) - t.penalty;
                score += 0.12f * b[(size_t) root];     // la basse confirme la fondamentale
                if (score > bestScore) { bestScore = score; bestRoot = root; best = &t; }
            }
        }

        const juce::String name = pitchName (bestRoot) + best->quality;
        if (! r.chords.empty() && r.chords.back().root == bestRoot)
        {
            // Même fondamentale que la fenêtre précédente : même accord ; on garde
            // la qualité la plus simple (triade) si l'une des deux en est une.
            auto& prev = r.chords.back();
            if (best->intervals.size() < 4 && prev.quality.length() >= 2 && prev.quality != "m")
            {
                prev.quality = best->quality;
                prev.name = name;
            }
        }
        else
        {
            r.chords.push_back ({ (double) w, bestRoot, best->quality, name, bestScore });
        }

        totalWeight += wRms;
        if (best->minorish) minorWeight += wRms;
    }

    // --- Tonalité (Krumhansl) ---
    {
        float bestCorr = -2.0f;
        for (int root = 0; root < 12; ++root)
        {
            const float cMaj = correlation (globalChroma, majorProfile, root);
            const float cMin = correlation (globalChroma, minorProfile, root);
            if (cMaj > bestCorr) { bestCorr = cMaj; r.keyRoot = root; r.keyMinor = false; }
            if (cMin > bestCorr) { bestCorr = cMin; r.keyRoot = root; r.keyMinor = true; }
        }

        // Majeur / relatif mineur partagent les mêmes notes : on tranche avec le
        // premier accord (une boucle commence en général sur la tonique).
        if (! r.chords.empty())
        {
            const auto& first = r.chords.front();
            const bool firstMinor = first.quality == "m" || first.quality == "m7";
            if (! r.keyMinor && firstMinor && first.root == (r.keyRoot + 9) % 12)
            {
                r.keyRoot = first.root;
                r.keyMinor = true;
            }
            else if (r.keyMinor && ! firstMinor && first.root == (r.keyRoot + 3) % 12)
            {
                r.keyRoot = first.root;
                r.keyMinor = false;
            }
        }
        r.keyName = pitchName (r.keyRoot) + (r.keyMinor ? " minor" : " major");
    }

    // --- Descripteurs ---
    const float centroid = centroidWeight > 0.0 ? (float) (centroidSum / centroidWeight) : 1000.0f;
    const float centroidNorm = juce::jlimit (0.0f, 1.0f, std::log2 (centroid / 200.0f) / std::log2 (6000.0f / 200.0f));
    const float minorFrac = totalWeight > 0.0f ? minorWeight / totalWeight : 0.5f;

    r.brightness = juce::jlimit (0.0f, 1.0f, 0.5f * centroidNorm + 0.5f * (1.0f - minorFrac));
    r.darkness   = juce::jlimit (0.0f, 1.0f, 0.55f * minorFrac + (r.keyMinor ? 0.25f : 0.0f) + 0.2f * (1.0f - centroidNorm));

    const float meanDb = juce::Decibels::gainToDecibels (totalRms, -100.0f);
    r.energy = juce::jlimit (0.0f, 1.0f, juce::jmap (meanDb, -45.0f, -8.0f, 0.0f, 1.0f));

    // Densité : pics du flux spectral au-dessus d'un seuil adaptatif
    if (flux.size() > 2)
    {
        const float mean = std::accumulate (flux.begin(), flux.end(), 0.0f) / (float) flux.size();
        float var = 0.0f;
        for (auto f : flux) var += (f - mean) * (f - mean);
        const float sd = std::sqrt (var / (float) flux.size());
        const float thresh = mean + 1.0f * sd;
        int onsets = 0;
        for (size_t i = 1; i + 1 < flux.size(); ++i)
            if (flux[i] > thresh && flux[i] >= flux[i - 1] && flux[i] > flux[i + 1])
                ++onsets;
        const float onsetsPerBeat = (float) onsets / (float) numBeats;
        r.density = juce::jlimit (0.0f, 1.0f, onsetsPerBeat / 3.0f);
    }

    return r;
}
