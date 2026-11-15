// Test console : synthétise une progression d'accords connue, lance l'analyse
// puis le générateur, et affiche le résultat.
//   cmake -B build -DARISTOTE_BUILD_TESTS=ON && cmake --build build --config Release --target AristoteTests

#include "analysis/ChordAnalyzer.h"
#include "generation/TrapGenerator.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>

static std::vector<float> synthProgression (const std::vector<std::vector<int>>& chords, double bpm, double sr,
                                            int beatsPerChord, float level)
{
    const int samplesPerChord = (int) (sr * 60.0 / bpm * beatsPerChord);
    std::vector<float> out ((size_t) samplesPerChord * chords.size(), 0.0f);
    for (size_t c = 0; c < chords.size(); ++c)
    {
        for (int midi : chords[c])
        {
            const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            for (int i = 0; i < samplesPerChord; ++i)
            {
                const double t = i / sr;
                const double env = std::exp (-1.5 * t) * 0.7 + 0.3;
                const double s = std::sin (2.0 * juce::MathConstants<double>::pi * f * t)
                               + 0.4 * std::sin (4.0 * juce::MathConstants<double>::pi * f * t)
                               + 0.2 * std::sin (6.0 * juce::MathConstants<double>::pi * f * t);
                out[(size_t) c * (size_t) samplesPerChord + (size_t) i] += (float) (s * env * level);
            }
        }
    }
    return out;
}

static int run (const char* title, const std::vector<std::vector<int>>& chords, float level)
{
    const double sr = 44100.0, bpm = 140.0;
    auto audio = synthProgression (chords, bpm, sr, 4, level);
    auto r = ChordAnalyzer::analyze (audio.data(), (int) audio.size(), sr, bpm, 4);

    std::cout << "\n=== " << title << " ===\n"
              << "key: " << r.keyName << "\nchords: " << r.chordProgressionText (8) << "\n"
              << "darkness " << r.darkness << "  brightness " << r.brightness
              << "  energy " << r.energy << "  density " << r.density << "\n";

    int failures = 0;
    auto presets = TrapGenerator::generate (r, 4, 1234u);
    for (const auto& p : presets)
    {
        std::cout << "  - " << p.name << " [" << moodToString (p.mood) << ", " << p.archetype << ", "
                  << p.lengthBeats << " beats, vol pts " << p.lanes[laneVolume].points.size() << "]\n    "
                  << p.note << "\n";
        for (const auto& lane : p.lanes)
        {
            if (lane.points.front().x != 0.0f || lane.points.back().x != 1.0f) ++failures;
            for (size_t i = 1; i < lane.points.size(); ++i)
                if (lane.points[i].x < lane.points[i - 1].x) ++failures;
            for (int k = 0; k < 1000; ++k)
            {
                const float v = lane.evaluate ((float) k / 1000.0f);
                if (! (v >= 0.0f && v <= 1.0f)) ++failures;
            }
        }
    }
    return failures;
}

int main()
{
    // A3=57 C4=60 E4=64 ...
    int failures = 0;
    failures += run ("Am - F - C - G (dark trap)",
                     { { 45, 57, 60, 64 }, { 41, 53, 57, 60 }, { 48, 55, 60, 64 }, { 43, 55, 59, 62 } }, 0.15f);
    failures += run ("C - G - Am - F (brighter)",
                     { { 48, 60, 64, 67 }, { 43, 55, 59, 62 }, { 45, 57, 60, 64 }, { 41, 53, 57, 60 } }, 0.15f);
    failures += run ("Silence", { { } }, 0.0f);

    std::cout << "\n" << (failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED: " + std::to_string (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
