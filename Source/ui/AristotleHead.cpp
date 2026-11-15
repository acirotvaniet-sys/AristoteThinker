#include "AristotleHead.h"
#include "AristoteLookAndFeel.h"
#include <cmath>

using juce::Path;
using juce::AffineTransform;

namespace
{
    constexpr float W     = 0.0055f;   // trait principal (unités normalisées)
    constexpr float Thin  = 0.0032f;
    constexpr float Bold  = 0.0095f;
    constexpr float pi    = juce::MathConstants<float>::pi;
}

AristotleHead::AristotleHead()
{
    cur = tgt = exprFor (Mood::Neutral);
    moodWeight[(size_t) Mood::Neutral] = 1.0f;

    // Boucles de barbe : positions pseudo-aléatoires mais déterministes
    juce::Random rng (7);
    for (int k = 0; k < 400 && beardCurls.size() < 70; ++k)
    {
        const float x = 0.34f + 0.32f * rng.nextFloat();
        const float y = 0.47f + 0.19f * rng.nextFloat();
        const float t = (y - 0.47f) / 0.21f;
        const float halfW = 0.165f * (1.0f - t * t * 0.6f);
        const float mx = (x - 0.5f) / 0.085f, my = (y - 0.515f) / 0.05f;   // bouche dégagée
        if (std::abs (x - 0.5f) > halfW || mx * mx + my * my < 1.0f)
            continue;
        beardCurls.push_back ({ x, y, 0.010f + 0.008f * rng.nextFloat(), rng.nextFloat() * 2.0f * pi });
    }

    // Boucles de cheveux le long du crâne (deux rangées)
    for (int row = 0; row < 2; ++row)
    {
        const float k = row == 0 ? 1.0f : 0.9f;
        const int n = row == 0 ? 15 : 11;
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::jmap ((float) i, 0.0f, (float) (n - 1), -1.75f, 1.75f);
            const float x = 0.5f + 0.195f * k * std::sin (a);
            const float y = 0.30f - 0.205f * k * std::cos (a);
            if (row == 1 && y > 0.2f) continue;   // front dégagé
            hairCurls.push_back ({ x, y, 0.016f + 0.004f * (float) ((i * 7) % 3), a + (row == 0 ? 0.0f : 1.4f) });
        }
    }

    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

AristotleHead::Expr AristotleHead::exprFor (Mood m)
{
    Expr e;
    switch (m)
    {
        case Mood::Neutral: break;
        case Mood::Focused:
            e = { -0.45f, 0.10f, 0.55f, 0.0f, 0.10f, -0.10f, 0.0f, 0.85f, 0.0f, 0.70f, 0.10f }; break;
        case Mood::Pensive:
            e = { 0.55f, -0.20f, 0.70f, -0.55f, -0.70f, -0.20f, 0.0f, 0.90f, 0.06f, 0.90f, 0.0f }; break;
        case Mood::Angry:
            e = { -1.0f, 0.45f, 0.60f, 0.0f, 0.15f, -0.75f, 0.25f, 1.05f, -0.02f, 0.50f, 1.0f }; break;
        case Mood::Serene:
            e = { 0.20f, 0.10f, 0.08f, 0.0f, 0.0f, 0.40f, 0.0f, 1.0f, -0.05f, 0.0f, 0.0f }; break;
        case Mood::Playful:
            e = { 0.60f, 0.50f, 1.0f, 0.40f, -0.10f, 0.85f, 0.35f, 1.10f, 0.08f, 0.30f, 0.20f }; break;
    }
    return e;
}

void AristotleHead::setMood (Mood m)
{
    mood = m;
    tgt = exprFor (m);
}

void AristotleHead::setMoodImmediately (Mood m)
{
    setMood (m);
    cur = tgt;
    moodWeight.fill (0.0f);
    moodWeight[(size_t) m] = 1.0f;
    repaint();
}

void AristotleHead::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastMs) * 0.001);
    lastMs = now;
    time += dt;

    // Interpolation exponentielle vers l'expression cible
    const float k = 1.0f - std::exp (-dt * 5.0f);
    auto approach = [k] (float& a, float b) { a += (b - a) * k; };
    approach (cur.browInner, tgt.browInner);
    approach (cur.browOuter, tgt.browOuter);
    approach (cur.eyeOpen, tgt.eyeOpen);
    approach (cur.pupilX, tgt.pupilX);
    approach (cur.pupilY, tgt.pupilY);
    approach (cur.mouthCurve, tgt.mouthCurve);
    approach (cur.mouthOpen, tgt.mouthOpen);
    approach (cur.mouthWidth, tgt.mouthWidth);
    approach (cur.tilt, tgt.tilt);
    approach (cur.forehead, tgt.forehead);
    approach (cur.nostril, tgt.nostril);
    for (size_t i = 0; i < moodWeight.size(); ++i)
        approach (moodWeight[i], i == (size_t) mood ? 1.0f : 0.0f);

    // Clignements
    if (time >= nextBlink)
    {
        blinkStart = time;
        nextBlink = time + 2.5 + juce::Random::getSystemRandom().nextFloat() * 3.5;
    }
    const float bp = (float) ((time - blinkStart) / 0.16);
    blink = (bp >= 0.0f && bp <= 1.0f) ? std::sin (pi * bp) : 0.0f;

    repaint();
}

void AristotleHead::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const float s = juce::jmin (area.getWidth(), area.getHeight() / 1.08f);
    const float ox = area.getCentreX() - s * 0.5f;
    const float oy = area.getY() + (area.getHeight() - s * 1.08f) * 0.5f;
    const auto base = AffineTransform::scale (s).translated (ox, oy);

    // Expression effective = expression interpolée + micro-mouvements
    Expr e = cur;
    const float t = (float) time;
    e.pupilX += moodWeight[(size_t) Mood::Focused] * std::sin (t * 1.6f) * 0.6f;          // il « lit » le son
    e.tilt   += (moodWeight[(size_t) Mood::Serene] + moodWeight[(size_t) Mood::Pensive]) * std::sin (t * 0.6f) * 0.015f;
    const float jitter = moodWeight[(size_t) Mood::Angry] * std::sin (t * 41.0f) * 0.0025f;
    const float bob = moodWeight[(size_t) Mood::Playful] * std::sin (t * 2.4f) * 0.006f
                    + std::sin (t * 0.9f) * 0.0015f;                                         // respiration

    const auto headT = AffineTransform::rotation (e.tilt, 0.5f, 0.72f).translated (jitter, bob).followedBy (base);
    const auto mirrorT = AffineTransform (-1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f).followedBy (headT);

    // Les épaisseurs sont normalisées : PathStrokeType s'applique APRÈS la
    // transformation, donc on les remet à l'échelle en pixels.
    auto stroke = [&g, s] (const Path& p, float w, const AffineTransform& tr, float alpha = 1.0f)
    {
        g.setColour (Ink::ink.withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
        g.strokePath (p, juce::PathStrokeType (juce::jmax (0.6f, w * s), juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded), tr);
    };
    auto both = [&] (const Path& p, float w, float alpha = 1.0f)
    {
        stroke (p, w, headT, alpha);
        stroke (p, w, mirrorT, alpha);
    };

    //=== Buste, toge, socle (ne s'inclinent pas) ===============================
    {
        Path bust;
        bust.startNewSubPath (0.06f, 1.0f);  bust.cubicTo (0.10f, 0.86f, 0.26f, 0.79f, 0.415f, 0.765f);
        bust.startNewSubPath (0.94f, 1.0f);  bust.cubicTo (0.90f, 0.86f, 0.74f, 0.79f, 0.585f, 0.765f);
        stroke (bust, W, base);

        Path folds;
        folds.startNewSubPath (0.20f, 0.84f);  folds.cubicTo (0.35f, 0.86f, 0.50f, 0.92f, 0.66f, 1.0f);
        folds.startNewSubPath (0.30f, 0.80f);  folds.cubicTo (0.42f, 0.84f, 0.55f, 0.90f, 0.74f, 1.0f);
        folds.startNewSubPath (0.42f, 0.775f); folds.cubicTo (0.50f, 0.80f, 0.60f, 0.86f, 0.82f, 0.95f);
        folds.startNewSubPath (0.15f, 0.92f);  folds.quadraticTo (0.30f, 0.93f, 0.42f, 1.0f);
        folds.startNewSubPath (0.70f, 0.82f);  folds.quadraticTo (0.78f, 0.86f, 0.86f, 0.93f);
        stroke (folds, Thin, base, 0.8f);

        Path plinth;
        plinth.startNewSubPath (0.04f, 1.0f); plinth.lineTo (0.96f, 1.0f);
        plinth.addRectangle (0.30f, 1.0f, 0.40f, 0.06f);
        stroke (plinth, W, base);

        auto label = juce::Rectangle<float> (0.30f, 1.0f, 0.40f, 0.06f).transformedBy (base);
        g.setColour (Ink::ink);
        g.setFont (Type::caps (juce::jmax (8.0f, s * 0.026f)));
        g.drawText ("ARISTOTELES", label, juce::Justification::centred);
    }

    //=== Cou ==================================================================
    {
        Path neck;
        neck.startNewSubPath (0.425f, 0.60f);
        neck.quadraticTo (0.42f, 0.70f, 0.415f, 0.765f);
        both (neck, W);
    }

    //=== Crâne, joues, oreilles ===============================================
    {
        Path skull;
        skull.addCentredArc (0.5f, 0.30f, 0.195f, 0.205f, 0.0f, -1.85f, 1.85f, true);
        stroke (skull, W, headT);

        Path cheek;
        cheek.startNewSubPath (0.313f, 0.357f);
        cheek.cubicTo (0.312f, 0.40f, 0.32f, 0.44f, 0.335f, 0.47f);
        both (cheek, W);

        Path ear;
        ear.startNewSubPath (0.313f, 0.33f);
        ear.cubicTo (0.285f, 0.32f, 0.275f, 0.39f, 0.318f, 0.41f);
        ear.startNewSubPath (0.305f, 0.345f);
        ear.quadraticTo (0.29f, 0.37f, 0.307f, 0.395f);
        both (ear, Thin);

        Path hair;
        for (const auto& c : hairCurls)
            hair.addCentredArc (c.x, c.y, c.r, c.r, c.a, 0.0f, 3.8f, true);
        stroke (hair, Thin, headT, 0.9f);
    }

    //=== Rides du front =======================================================
    if (e.forehead > 0.02f)
    {
        Path lines;
        for (int j = 0; j < 3; ++j)
        {
            const float y = 0.215f + (float) j * 0.017f;
            const float hw = 0.075f - (float) j * 0.012f;
            lines.startNewSubPath (0.5f - hw, y);
            lines.quadraticTo (0.5f, y - 0.008f - e.browInner * 0.007f, 0.5f + hw, y);
        }
        stroke (lines, Thin, headT, e.forehead * 0.85f);
    }

    //=== Sourcils =============================================================
    {
        Path brow;
        const float innerY = 0.300f - e.browInner * 0.030f;
        const float outerY = 0.294f - e.browOuter * 0.022f;
        brow.startNewSubPath (0.365f, outerY);
        brow.quadraticTo (0.415f, 0.284f - (e.browInner + e.browOuter) * 0.012f, 0.472f, innerY);
        both (brow, Bold);
    }

    //=== Yeux (sans miroir : le regard part dans la même direction) ============
    for (const float cx : { 0.415f, 0.585f })
    {
        const float cy = 0.345f, hw = 0.037f;
        const float open = e.eyeOpen * (1.0f - blink);

        if (open < 0.15f)
        {
            Path closed;   // yeux fermés en arc (sérénité)
            closed.startNewSubPath (cx - hw, cy);
            closed.quadraticTo (cx, cy + 0.012f, cx + hw, cy);
            stroke (closed, W, headT);
            continue;
        }

        const float hU = 0.017f * open + 0.004f;
        const float hL = 0.008f;
        Path eye;
        eye.startNewSubPath (cx - hw, cy);
        eye.quadraticTo (cx, cy - 2.0f * hU, cx + hw, cy);
        eye.quadraticTo (cx, cy + 2.0f * hL, cx - hw, cy);
        eye.closeSubPath();

        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (eye, headT);
            Path pupil;
            const float px = cx + e.pupilX * hw * 0.5f;
            const float py = cy - hU * 0.2f + e.pupilY * 0.006f;
            pupil.addEllipse (px - 0.0115f, py - 0.0115f, 0.023f, 0.023f);
            g.setColour (Ink::ink);
            g.fillPath (pupil, headT);
        }
        stroke (eye, W, headT);

        Path crease;
        crease.startNewSubPath (cx - hw * 0.8f, cy - hU - 0.008f);
        crease.quadraticTo (cx, cy - 2.0f * hU - 0.006f, cx + hw * 0.9f, cy - hU - 0.006f);
        stroke (crease, Thin, headT, 0.7f);

        Path under;
        under.startNewSubPath (cx - hw * 0.6f, cy + 0.02f);
        under.quadraticTo (cx, cy + 0.028f, cx + hw * 0.6f, cy + 0.02f);
        stroke (under, Thin, headT, 0.2f + e.forehead * 0.4f);
    }

    //=== Nez ==================================================================
    {
        Path nose;
        nose.startNewSubPath (0.481f, 0.325f);
        nose.cubicTo (0.478f, 0.37f, 0.468f, 0.40f, 0.462f, 0.432f);
        nose.startNewSubPath (0.485f, 0.455f);
        nose.quadraticTo (0.5f, 0.462f, 0.515f, 0.455f);
        stroke (nose, W, headT);

        Path ridge;
        ridge.startNewSubPath (0.522f, 0.36f);
        ridge.lineTo (0.528f, 0.425f);
        stroke (ridge, Thin, headT, 0.35f);

        const float flare = 1.0f + 0.25f * e.nostril;
        Path nostril;
        nostril.startNewSubPath (0.5f - 0.042f * flare, 0.428f);
        nostril.quadraticTo (0.5f - 0.048f * flare, 0.45f, 0.475f, 0.452f);
        both (nostril, W);

        Path fold;   // sillons naso-géniens
        const float ac = std::abs (e.mouthCurve);
        fold.startNewSubPath (0.452f, 0.445f);
        fold.quadraticTo (0.43f, 0.47f, 0.425f - 0.01f * ac, 0.50f + 0.005f * e.mouthCurve);
        both (fold, Thin, 0.3f + 0.5f * ac);

        Path cheekbone;
        cheekbone.startNewSubPath (0.345f, 0.40f);
        cheekbone.quadraticTo (0.36f, 0.415f, 0.375f, 0.418f);
        both (cheekbone, Thin, 0.5f);
    }

    //=== Barbe (masque le cou) ================================================
    {
        Path beard;
        beard.startNewSubPath (0.335f, 0.43f);
        beard.cubicTo (0.33f, 0.56f, 0.40f, 0.655f, 0.5f, 0.67f);
        beard.cubicTo (0.60f, 0.655f, 0.67f, 0.56f, 0.665f, 0.43f);
        beard.quadraticTo (0.60f, 0.47f, 0.5f, 0.465f);
        beard.quadraticTo (0.40f, 0.47f, 0.335f, 0.43f);
        beard.closeSubPath();
        g.setColour (Ink::paper);
        g.fillPath (beard, headT);

        Path outline;
        outline.startNewSubPath (0.335f, 0.45f);
        outline.cubicTo (0.33f, 0.56f, 0.40f, 0.655f, 0.5f, 0.67f);
        outline.cubicTo (0.60f, 0.655f, 0.67f, 0.56f, 0.665f, 0.45f);
        stroke (outline, W, headT);

        Path curls;
        for (const auto& c : beardCurls)
            curls.addCentredArc (c.x, c.y, c.r, c.r, c.a, 0.0f, 3.6f, true);
        stroke (curls, Thin, headT, 0.85f);
    }

    //=== Moustache ============================================================
    {
        const float droop = e.mouthCurve * 0.008f;
        Path m;
        m.startNewSubPath (0.497f, 0.466f);
        m.cubicTo (0.47f, 0.458f, 0.445f, 0.468f, 0.428f, 0.497f - droop);
        m.startNewSubPath (0.49f, 0.474f);
        m.quadraticTo (0.465f, 0.472f, 0.44f, 0.495f - droop);
        both (m, W);
    }

    //=== Bouche ===============================================================
    {
        const float cy = 0.508f;
        const float hw = 0.046f * e.mouthWidth;
        const float cornerY = cy - e.mouthCurve * 0.018f;
        const float upperC = cy + e.mouthCurve * 0.012f - 0.002f;

        Path mouth;
        mouth.startNewSubPath (0.5f - hw, cornerY);
        mouth.quadraticTo (0.5f, upperC, 0.5f + hw, cornerY);
        if (e.mouthOpen > 0.03f)
        {
            mouth.quadraticTo (0.5f, upperC + e.mouthOpen * 0.055f + 0.004f, 0.5f - hw, cornerY);
            mouth.closeSubPath();
            g.setColour (Ink::ink);
            g.fillPath (mouth, headT);
        }
        stroke (mouth, W, headT);

        Path lip;
        const float ly = cy + 0.02f + e.mouthOpen * 0.045f;
        lip.startNewSubPath (0.5f - hw * 0.5f, ly);
        lip.quadraticTo (0.5f, ly + 0.006f, 0.5f + hw * 0.5f, ly);
        stroke (lip, Thin, headT, 0.6f);
    }

    //=== Effets d'humeur ======================================================
    if (const float w = moodWeight[(size_t) Mood::Focused]; w > 0.01f)
    {
        // Bulles de pensée qui pulsent l'une après l'autre
        const float pts[3][3] { { 0.76f, 0.14f, 0.008f }, { 0.81f, 0.09f, 0.011f }, { 0.87f, 0.045f, 0.015f } };
        for (int i = 0; i < 3; ++i)
        {
            const float a = 0.25f + 0.75f * (0.5f + 0.5f * std::sin (t * 3.0f - (float) i * 0.9f));
            Path bubble;
            bubble.addEllipse (pts[i][0] - pts[i][2], pts[i][1] - pts[i][2], pts[i][2] * 2.0f, pts[i][2] * 2.0f);
            stroke (bubble, Thin, base, w * a);
        }
    }

    if (const float w = moodWeight[(size_t) Mood::Angry]; w > 0.01f)
    {
        // Petits traits de colère près des tempes
        Path marks;
        for (int i = 0; i < 3; ++i)
        {
            const float a = -0.9f + (float) i * 0.35f;
            const float r0 = 0.25f, r1 = 0.29f + 0.01f * std::sin (t * 9.0f + (float) i);
            marks.startNewSubPath (0.5f + r0 * std::sin (a), 0.30f - r0 * std::cos (a));
            marks.lineTo (0.5f + r1 * std::sin (a), 0.30f - r1 * std::cos (a));
        }
        both (marks, W, w);
    }

    if (const float w = moodWeight[(size_t) Mood::Serene]; w > 0.01f)
    {
        // Halo discret
        Path halo;
        halo.addCentredArc (0.5f, 0.10f, 0.12f, 0.025f, 0.0f, 0.0f, 2.0f * pi, true);
        stroke (halo, Thin, headT, w * (0.45f + 0.2f * std::sin (t * 1.2f)));
    }
}
