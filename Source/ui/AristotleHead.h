#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "model/Preset.h"
#include <array>

// Buste d'Aristote dessiné en vectoriel (Path JUCE), coordonnées normalisées.
// Chaque humeur est un jeu de paramètres faciaux (sourcils, paupières, regard,
// bouche, inclinaison...) ; on interpole en continu vers la cible -> transitions
// animées. S'y ajoutent clignements, respiration et effets propres à l'humeur.
class AristotleHead : public juce::Component, private juce::Timer
{
public:
    AristotleHead();

    void setMood (Mood m);
    void setMoodImmediately (Mood m);   // sans transition (tests, rendu statique)
    Mood getMood() const { return mood; }

    void paint (juce::Graphics& g) override;

private:
    struct Expr
    {
        float browInner = 0, browOuter = 0, eyeOpen = 0.8f, pupilX = 0, pupilY = 0;
        float mouthCurve = 0, mouthOpen = 0, mouthWidth = 1, tilt = 0, forehead = 0.2f, nostril = 0;
    };

    struct Curl { float x, y, r, a; };

    static Expr exprFor (Mood m);
    void timerCallback() override;

    Mood mood = Mood::Neutral;
    Expr cur, tgt;
    std::array<float, 6> moodWeight {};   // fondu des effets spécifiques à chaque humeur
    double time = 0.0, lastMs = 0.0;
    double nextBlink = 2.0, blinkStart = -10.0;
    float blink = 0.0f;
    std::vector<Curl> beardCurls, hairCurls;
};
