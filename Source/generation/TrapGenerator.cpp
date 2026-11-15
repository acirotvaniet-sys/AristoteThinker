#include "TrapGenerator.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace
{
    constexpr double kEps = 0.004;   // ~2 ms à 140 BPM : micro-fondu anti-clic

    //==========================================================================
    // Construit une courbe séquentiellement, en unités de temps (beats).
    class LaneBuilder
    {
    public:
        LaneBuilder (double lengthBeats, float initial) : len (lengthBeats)
        {
            pts.push_back ({ 0.0, initial, 0.0f });
        }

        float last() const { return pts.back().v; }

        // Ajoute un point ; le segment précédent y mène en douceur (rampe).
        void set (double b, float v, float curve = 0.0f)
        {
            b = juce::jlimit (0.0, len, b);
            v = juce::jlimit (0.0f, 1.0f, v);
            if (b <= pts.back().b + 1.0e-9)
            {
                pts.back().v = v;
                pts.back().c = curve;
                return;
            }
            pts.push_back ({ b, v, curve });
        }

        // Changement instantané : tient la valeur précédente jusqu'à b, puis saute.
        void step (double b, float v, float curve = 0.0f)
        {
            b = juce::jlimit (0.0, len, b);
            const auto lp = pts.back();
            if (b - lp.b > kEps)
                pts.push_back ({ b - kEps, lp.v, 0.0f });
            set (b, v, curve);
        }

        Lane finish (float defaultValue) const
        {
            Lane lane;
            for (const auto& p : pts)
                lane.points.push_back ({ (float) (p.b / len), p.v, p.c });
            lane.sanitize (defaultValue);
            return lane;
        }

    private:
        struct P { double b; float v; float c; };
        std::vector<P> pts;
        double len;
    };

    //==========================================================================
    // Évènements : chacun dessine dans [t, end[ où end = début de l'évènement suivant.
    using DrawFn = std::function<void (LaneBuilder&, double t, double end)>;
    struct Ev { double t; DrawFn draw; };

    Lane buildLane (double len, float initial, std::vector<Ev> evs, float def)
    {
        std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
        evs.erase (std::remove_if (evs.begin(), evs.end(), [len] (const Ev& e) { return e.t < 0.0 || e.t >= len; }), evs.end());

        LaneBuilder lb (len, initial);
        for (size_t i = 0; i < evs.size(); ++i)
        {
            const double end = i + 1 < evs.size() ? evs[i + 1].t : len;
            if (end - evs[i].t > 1.0e-6)
                evs[i].draw (lb, evs[i].t, end);
        }
        return lb.finish (def);
    }

    // Pompage sidechain : chute instantanée puis remontée courbée.
    DrawFn pump (float floor, double release, float curve)
    {
        return [=] (LaneBuilder& lb, double t, double end)
        {
            lb.step (t, floor, curve);
            const double r = std::min (release, end - t - kEps);
            if (r > kEps)
                lb.set (t + r, 1.0f);
        };
    }

    // Gate : ouvert à `on`, décroît éventuellement vers `decayTo`, puis retombe à `off`.
    DrawFn gate (float on, double length, float off, float decayTo = -1.0f, float curve = 0.0f)
    {
        return [=] (LaneBuilder& lb, double t, double end)
        {
            const double l = std::min (length, end - t - kEps);
            lb.step (t, on, curve);
            if (l <= 2.0 * kEps)
                return;
            if (decayTo >= 0.0f)
                lb.set (t + l - kEps, decayTo);
            lb.step (t + l, off);
        };
    }

    // Roll façon hi-hat : répétitions rapides avec crescendo/decrescendo.
    DrawFn roll (double sub, float from, float to, float duty, float off)
    {
        return [=] (LaneBuilder& lb, double t, double end)
        {
            const double span = end - t;
            for (double s = t; s < end - 2.0 * kEps; s += sub)
            {
                const float a = (float) ((s - t) / span);
                lb.step (s, from + (to - from) * a);
                const double gl = std::min (sub * duty, end - s - kEps);
                if (gl > kEps)
                    lb.step (s + gl, off);
            }
        };
    }

    DrawFn silence()
    {
        return [] (LaneBuilder& lb, double t, double) { lb.step (t, 0.0f); };
    }

    //==========================================================================
    struct Ctx
    {
        float d = 0.5f, br = 0.5f, e = 0.5f, n = 0.5f;
        double bpm = 140.0;
        double hat = 0.25;         // pas « hi-hat » : 1/16 à tempo normal, 1/32 si écrit en half-time
        std::vector<double> chordBeats;
        juce::Random rng;
        const AnalysisResult* analysis = nullptr;

        std::vector<double> chordChanges (double len) const
        {
            std::vector<double> out;
            for (auto b : chordBeats)
            {
                const double m = std::fmod (b, len);
                if (m > 0.01 && std::none_of (out.begin(), out.end(), [m] (double x) { return std::abs (x - m) < 0.1; }))
                    out.push_back (m);
            }
            std::sort (out.begin(), out.end());
            return out;
        }

        juce::String heard() const
        {
            if (analysis == nullptr || ! analysis->valid)  return "No analysis yet";
            if (analysis->silent)                          return "Heard silence";
            return "Heard " + analysis->keyName + " (" + analysis->chordProgressionText (4) + ")";
        }
    };

    int pct (float v) { return juce::roundToInt (v * 100.0f); }

    //==========================================================================
    Preset halfTimePump (Ctx& c)
    {
        Preset p;
        const double L = c.d > 0.45f ? 8.0 : 4.0;
        p.lengthBeats = L;

        static const std::vector<std::vector<double>> kickTemplates {
            { 0.0, 1.5, 2.5 }, { 0.0, 0.75, 2.5, 3.25 }, { 0.0, 2.25, 2.75 }, { 0.0, 1.75, 3.5 }, { 0.0, 2.5 }
        };

        const auto& tplA = kickTemplates[(size_t) c.rng.nextInt ((int) kickTemplates.size())];
        const auto& tplB = c.rng.nextFloat() < 0.5f ? kickTemplates[(size_t) c.rng.nextInt ((int) kickTemplates.size())] : tplA;

        std::vector<double> kicks;
        for (int bar = 0; bar < (int) (L / 4.0); ++bar)
            for (auto k : (bar % 2 == 0 ? tplA : tplB))
                kicks.push_back (bar * 4.0 + k);

        bool accents = false;
        for (auto cc : c.chordChanges (L))
            if (std::none_of (kicks.begin(), kicks.end(), [cc] (double k) { return std::abs (k - cc) < 0.3; }))
            {
                kicks.push_back (cc);
                accents = true;
            }
        std::sort (kicks.begin(), kicks.end());

        const float floor  = juce::jlimit (0.02f, 0.4f, 0.32f - 0.27f * c.e - 0.08f * c.d);
        const double release = 0.45 + 0.6 * c.d;
        const float curve  = 0.35f + 0.3f * c.d;
        const bool dropout = c.rng.nextFloat() < 0.35f + 0.4f * c.d;
        const double gap   = c.d > 0.6f ? 0.5 : 0.25;

        std::vector<Ev> ev;
        for (auto k : kicks)
            if (! dropout || k < L - gap - 1.0e-6)
                ev.push_back ({ k, pump (floor, release, curve) });
        if (dropout)
            ev.push_back ({ L - gap, silence() });
        p.lanes[laneVolume] = buildLane (L, 1.0f, ev, 1.0f);

        // Filtre : se referme lentement, se rouvre avant la reprise
        LaneBuilder f (L, 1.0f);
        const float base = 0.35f + 0.3f * (1.0f - c.d);
        const int bars = (int) (L / 4.0);
        for (int bar = 0; bar < bars; ++bar)
        {
            const double b0 = bar * 4.0;
            f.set (b0, base + 0.25f, 0.5f);
            if (bar == bars - 1)
            {
                f.set (b0 + 2.0, base, -0.4f);
                f.set (b0 + 4.0 - kEps, 1.0f);
            }
            else
                f.set (b0 + 3.5, base);
        }
        p.lanes[laneFilter] = f.finish (1.0f);

        // Pan : dérive lente
        LaneBuilder pn (L, 0.5f);
        const float a = 0.10f + 0.08f * c.br;
        pn.set (0.0, 0.5f, -0.4f);
        pn.set (L * 0.25, 0.5f + a, 0.4f);
        pn.set (L * 0.5, 0.5f, -0.4f);
        pn.set (L * 0.75, 0.5f - a, 0.4f);
        pn.set (L, 0.5f);
        p.lanes[lanePan] = pn.finish (0.5f);

        p.mood = c.d > 0.5f ? Mood::Pensive : (c.e > 0.65f ? Mood::Angry : Mood::Serene);
        p.note = c.heard() + ". Half-time 808 pump: ducks to " + juce::String (pct (floor)) + "% with a "
               + (release > 0.8 ? "long, heavy" : "short") + " release"
               + (dropout ? ", silence before the downbeat" : "")
               + (accents ? ", extra hits on chord changes" : "") + ".";
        return p;
    }

    Preset tripletBounce (Ctx& c)
    {
        Preset p;
        const double L = 4.0;
        p.lengthBeats = L;
        const double trip = c.hat * 4.0 / 3.0;    // 1/8T (ou 1/16T en half-time)
        const int steps = (int) std::lround (L / trip);

        static const juce::StringArray patterns { "101101101101", "100100101101", "110101101011", "101011010110" };
        juce::String pat = patterns[c.rng.nextInt (patterns.size())];
        std::vector<bool> on ((size_t) steps);
        for (int i = 0; i < steps; ++i)
        {
            bool v = pat[i % pat.length()] == '1';
            if (i > 0 && c.rng.nextFloat() < 0.08f + 0.1f * c.n) v = ! v;
            on[(size_t) i] = v;
        }
        on[0] = true;

        const float off = 0.12f + 0.25f * (1.0f - c.e);
        const bool endRoll = c.rng.nextFloat() < 0.5f;
        const double rollStart = endRoll ? L - 1.0 : L;

        std::vector<Ev> ev;
        LaneBuilder pn (L, 0.5f);
        const float w = 0.22f + 0.15f * c.br;
        bool left = true;
        for (int i = 0; i < steps; ++i)
        {
            const double t = i * trip;
            if (! on[(size_t) i] || t >= rollStart - 1.0e-6) continue;
            ev.push_back ({ t, gate (1.0f, trip * 0.85, off, 0.55f) });
            pn.step (t, left ? 0.5f - w : 0.5f + w);
            left = ! left;
        }
        if (endRoll)
        {
            ev.push_back ({ rollStart, roll (trip * 0.5, 0.5f, 1.0f, 0.5f, off) });
            pn.step (rollStart, 0.5f - w, 0.0f);
            pn.set (L - kEps, 0.5f + w);
        }
        p.lanes[laneVolume] = buildLane (L, off, ev, 1.0f);
        p.lanes[lanePan] = pn.finish (0.5f);

        LaneBuilder f (L, 1.0f);
        for (int b = 0; b < (int) L; ++b)
        {
            f.step ((double) b, 0.95f - 0.1f * c.d, 0.5f);
            f.set (b + 0.75, 0.55f - 0.15f * c.d);
        }
        p.lanes[laneFilter] = f.finish (1.0f);

        p.mood = c.d > 0.7f ? Mood::Pensive : Mood::Playful;
        p.note = c.heard() + ". Triplet bounce with ping-pong stereo"
               + juce::String (endRoll ? ", triplet roll into the next bar" : "") + ".";
        return p;
    }

    Preset hiHatRoll (Ctx& c)
    {
        Preset p;
        const double L = 4.0;
        p.lengthBeats = L;
        const double base = c.hat * 2.0;   // 1/8

        struct Spot { double s0, s1; };
        std::vector<Spot> spots;
        if (c.rng.nextFloat() < 0.5f + 0.3f * c.n)
            spots.push_back ({ 1.5, 2.0 });
        spots.push_back ({ c.e < 0.35f ? L - 0.5 : L - 1.0, L });

        const double subs[] { c.hat * 0.5, c.hat * 2.0 / 3.0, c.hat / 3.0 };
        const float off = juce::jlimit (0.0f, 0.6f, 0.45f - 0.35f * c.e);

        auto inSpot = [&] (double t)
        {
            return std::any_of (spots.begin(), spots.end(), [t] (const Spot& s) { return t >= s.s0 - 1.0e-6 && t < s.s1; });
        };

        std::vector<Ev> ev;
        for (double t = 0.0; t < L - 1.0e-6; t += base)
            if (! inSpot (t))
                ev.push_back ({ t, gate (1.0f, base * 0.55, off) });

        juce::StringArray rollDesc;
        for (const auto& s : spots)
        {
            const int si = c.rng.nextInt (3);
            const bool down = c.rng.nextFloat() < 0.3f;
            ev.push_back ({ s.s0, roll (subs[si], down ? 1.0f : 0.35f, down ? 0.35f : 1.0f, 0.5f, 0.0f) });
            rollDesc.add (juce::String (si == 0 ? "1/32" : si == 1 ? "1/16T" : "1/32T") + (down ? " fade-out" : " crescendo"));
        }
        p.lanes[laneVolume] = buildLane (L, 1.0f, ev, 1.0f);

        LaneBuilder f (L, 0.8f);
        LaneBuilder pn (L, 0.5f);
        for (const auto& s : spots)
        {
            f.step (s.s0, 0.35f, 0.4f);
            f.set (s.s1 - kEps, 1.0f);
            f.step (s.s1, 0.8f);
            pn.step (s.s0, 0.2f);
            pn.set (s.s1 - kEps, 0.8f);
            pn.step (s.s1, 0.5f);
        }
        p.lanes[laneFilter] = f.finish (1.0f);
        p.lanes[lanePan] = pn.finish (0.5f);

        p.mood = c.e > 0.55f ? Mood::Angry : Mood::Playful;
        p.note = c.heard() + ". Hi-hat style stutters (" + rollDesc.joinIntoString (", ")
               + ") over a 1/8 gate, filter and pan sweep on each roll.";
        return p;
    }

    Preset choppedGate (Ctx& c)
    {
        Preset p;
        const double L = 4.0;
        p.lengthBeats = L;
        const double g = c.hat;
        const double gateLen = g * (0.45 + 0.4 * (1.0 - c.e));

        std::vector<double> hits;
        for (int half = 0; half < 2; ++half)
        {
            const double h0 = half * 2.0;
            for (int s : { 0, 3, 6 })                       // tresillo 3-3-2
                hits.push_back (h0 + s * g);
            for (int s = 0; s < (int) std::lround (2.0 / g); ++s)
                if (s != 0 && s != 3 && s != 6 && c.rng.nextFloat() < 0.15f + 0.35f * c.n)
                    hits.push_back (h0 + s * g);
        }
        for (auto cc : c.chordChanges (L))
            hits.push_back (std::round (cc / g) * g);

        const bool stutter = c.rng.nextFloat() < 0.6f;
        const bool dropout = ! stutter && c.rng.nextFloat() < 0.5f;
        const double tailStart = stutter ? L - 0.5 : (dropout ? L - 2.0 * g : L);

        std::sort (hits.begin(), hits.end());
        hits.erase (std::unique (hits.begin(), hits.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }), hits.end());

        std::vector<Ev> ev;
        LaneBuilder pn (L, 0.5f);
        for (auto h : hits)
        {
            if (h >= tailStart - 1.0e-6) continue;
            ev.push_back ({ h, gate (1.0f, gateLen, 0.0f) });
            pn.step (h, 0.5f + (c.rng.nextFloat() - 0.5f) * 0.5f);
        }
        if (stutter)  ev.push_back ({ tailStart, roll (g * 0.5, 1.0f, 1.0f, 0.5f, 0.0f) });
        if (dropout)  ev.push_back ({ tailStart, silence() });
        p.lanes[laneVolume] = buildLane (L, 0.0f, ev, 1.0f);
        p.lanes[lanePan] = pn.finish (0.5f);

        LaneBuilder f (L, 1.0f);
        for (double t = 0.0; t < L - 1.0e-6; t += g * 2.0)
            f.step (t, 0.45f + 0.55f * c.rng.nextFloat());
        p.lanes[laneFilter] = f.finish (1.0f);

        p.mood = (c.e < 0.3f && c.d < 0.4f) ? Mood::Playful : Mood::Angry;
        p.note = c.heard() + ". Chopped 3-3-2 gate with hard cuts"
               + juce::String (stutter ? ", 1/32 stutter at the end of the bar" : dropout ? ", dropout before the downbeat" : "")
               + ", sample-and-hold filter.";
        return p;
    }

    Preset breathSwell (Ctx& c)
    {
        Preset p;
        const double L = 8.0;
        p.lengthBeats = L;
        const float low = 0.35f + 0.25f * (1.0f - c.e);

        std::vector<Ev> ev;
        for (double t = 0.0; t < L - 1.0e-6; t += 2.0)
            ev.push_back ({ t, [low] (LaneBuilder& lb, double t0, double end)
            {
                lb.step (t0, low, 0.6f);
                lb.set (t0 + std::min (1.5, (end - t0) * 0.75), 1.0f, -0.3f);
                lb.set (end - 2.0 * kEps, low + 0.15f);
            } });

        const bool dropout = c.d > 0.5f && c.rng.nextFloat() < 0.5f;
        if (dropout)
            ev.push_back ({ L - 0.25, silence() });
        p.lanes[laneVolume] = buildLane (L, 1.0f, ev, 1.0f);

        LaneBuilder f (L, 0.4f);
        f.set (0.0, 0.3f + 0.2f * c.br, 0.4f);
        f.set (6.0, 0.95f, -0.3f);
        f.set (L - kEps, 0.6f);
        p.lanes[laneFilter] = f.finish (1.0f);

        LaneBuilder pn (L, 0.5f);
        const float a = 0.3f;
        pn.set (0.0, 0.5f, -0.4f);
        pn.set (2.0, 0.5f + a, 0.4f);
        pn.set (4.0, 0.5f, -0.4f);
        pn.set (6.0, 0.5f - a, 0.4f);
        pn.set (L, 0.5f);
        p.lanes[lanePan] = pn.finish (0.5f);

        p.mood = c.d > 0.5f ? Mood::Pensive : Mood::Serene;
        p.note = c.heard() + ". Slow breathing swells every two beats, filter opening over two bars, wide pan"
               + juce::String (dropout ? ", a breath of silence before the drop" : "") + ".";
        return p;
    }

    //==========================================================================
    juce::String pickName (Mood mood, juce::Random& rng, std::set<juce::String>& used)
    {
        static const juce::StringArray pensiveA { "Brooding", "Silent", "Midnight", "Hollow", "Grave", "Stoic", "Distant", "Somber" };
        static const juce::StringArray pensiveN { "Syllogism", "Cave", "Doubt", "Ethics", "Shadow", "Metaphysics", "Premise", "Elegy" };
        static const juce::StringArray angryA   { "Furious", "Spartan", "Burning", "Savage", "Iron", "Molten", "Feral", "Thundering" };
        static const juce::StringArray angryN   { "Polemic", "Phalanx", "Tirade", "Wrath", "Rhetoric", "Siege", "Fallacy", "Tribunal" };
        static const juce::StringArray sereneA  { "Golden", "Gentle", "Still", "Lucid", "Calm", "Ionian", "Tender", "Slow" };
        static const juce::StringArray sereneN  { "Mean", "Breeze", "Harmony", "Lyceum", "Virtue", "Garden", "Ether", "Contemplation" };
        static const juce::StringArray playA    { "Bouncing", "Witty", "Dancing", "Restless", "Nimble", "Cheeky", "Giddy", "Wandering" };
        static const juce::StringArray playN    { "Agora", "Paradox", "Riddle", "Satyr", "Logic", "Muse", "Sophist", "Peripatetic" };

        const juce::StringArray* adj = &pensiveA;
        const juce::StringArray* noun = &pensiveN;
        switch (mood)
        {
            case Mood::Angry:   adj = &angryA;  noun = &angryN;  break;
            case Mood::Serene:  adj = &sereneA; noun = &sereneN; break;
            case Mood::Playful: adj = &playA;   noun = &playN;   break;
            default: break;
        }

        for (int attempt = 0; attempt < 40; ++attempt)
        {
            auto name = (*adj)[rng.nextInt (adj->size())] + " " + (*noun)[rng.nextInt (noun->size())];
            if (used.insert (name).second)
                return name;
        }
        auto name = (*adj)[0] + " " + (*noun)[0] + " " + juce::String (rng.nextInt (99) + 1);
        used.insert (name);
        return name;
    }

    Ctx makeCtx (const AnalysisResult& a, juce::uint32 seed)
    {
        Ctx c;
        c.analysis = &a;
        c.rng.setSeed ((juce::int64) seed);
        if (a.valid && ! a.silent)
        {
            c.d = a.darkness; c.br = a.brightness; c.e = a.energy; c.n = a.density;
        }
        c.bpm = a.bpm > 0.0 ? a.bpm : 140.0;
        c.hat = c.bpm >= 100.0 ? 0.25 : 0.125;
        for (const auto& ch : a.chords)
            c.chordBeats.push_back (ch.beat);
        return c;
    }
}

//==============================================================================
juce::String TrapGenerator::archetypeName (Archetype a)
{
    switch (a)
    {
        case Archetype::HalfTimePump:  return "HalfTimePump";
        case Archetype::TripletBounce: return "TripletBounce";
        case Archetype::HiHatRoll:     return "HiHatRoll";
        case Archetype::ChoppedGate:   return "ChoppedGate";
        case Archetype::BreathSwell:   return "BreathSwell";
        default: break;
    }
    return "HalfTimePump";
}

TrapGenerator::Archetype TrapGenerator::archetypeFromName (const juce::String& s)
{
    for (int i = 0; i < (int) Archetype::numArchetypes; ++i)
        if (archetypeName ((Archetype) i) == s)
            return (Archetype) i;
    return Archetype::HalfTimePump;
}

Preset TrapGenerator::generateOne (const AnalysisResult& analysis, Archetype archetype,
                                   juce::uint32 seed, std::set<juce::String>& usedNames)
{
    auto c = makeCtx (analysis, seed);
    Preset p;
    switch (archetype)
    {
        case Archetype::TripletBounce: p = tripletBounce (c); break;
        case Archetype::HiHatRoll:     p = hiHatRoll (c);     break;
        case Archetype::ChoppedGate:   p = choppedGate (c);   break;
        case Archetype::BreathSwell:   p = breathSwell (c);   break;
        case Archetype::HalfTimePump:
        default:                       p = halfTimePump (c);  break;
    }
    p.id = juce::Uuid().toString();
    p.archetype = archetypeName (archetype);
    p.seed = seed;
    p.name = pickName (p.mood, c.rng, usedNames);
    return p;
}

std::vector<Preset> TrapGenerator::generate (const AnalysisResult& analysis, int count, juce::uint32 seed)
{
    auto c = makeCtx (analysis, seed);

    // Poids des archétypes selon l'analyse (logique « producteur »)
    std::vector<float> w ((size_t) Archetype::numArchetypes);
    w[(size_t) Archetype::HalfTimePump]  = 0.5f + 1.2f * c.d + 0.3f * c.e;
    w[(size_t) Archetype::TripletBounce] = 0.3f + 1.3f * c.br;
    w[(size_t) Archetype::HiHatRoll]     = 0.3f + 1.0f * c.e + 0.5f * c.n;
    w[(size_t) Archetype::ChoppedGate]   = 0.2f + 1.2f * c.e;
    w[(size_t) Archetype::BreathSwell]   = 0.3f + 1.2f * (1.0f - c.e);

    std::vector<int> order;
    // 1er : l'archétype le plus pertinent
    order.push_back ((int) std::distance (w.begin(), std::max_element (w.begin(), w.end())));
    w[(size_t) order.back()] = 0.0f;

    count = juce::jlimit (1, (int) Archetype::numArchetypes, count);
    while ((int) order.size() < count)
    {
        float total = 0.0f;
        for (auto v : w) total += v;
        float r = c.rng.nextFloat() * total;
        int pick = 0;
        for (int i = 0; i < (int) w.size(); ++i)
        {
            r -= w[(size_t) i];
            if (r <= 0.0f && w[(size_t) i] > 0.0f) { pick = i; break; }
            if (w[(size_t) i] > 0.0f) pick = i;
        }
        order.push_back (pick);
        w[(size_t) pick] = 0.0f;
    }

    std::set<juce::String> used;
    std::vector<Preset> out;
    for (size_t i = 0; i < order.size(); ++i)
        out.push_back (generateOne (analysis, (Archetype) order[i], seed * 7919u + (juce::uint32) i * 104729u + 1u, used));
    return out;
}

Preset TrapGenerator::makeDefault()
{
    AnalysisResult none;
    std::set<juce::String> used;
    auto p = generateOne (none, Archetype::HalfTimePump, 1u, used);
    p.name = "First Principle";
    p.note = "Default half-time pump. Press ANALYZE while your loop plays to let Aristotle think.";
    return p;
}
