#include "Preset.h"
#include <algorithm>
#include <cmath>

juce::String moodToString (Mood m)
{
    switch (m)
    {
        case Mood::Neutral: return "Neutral";
        case Mood::Focused: return "Focused";
        case Mood::Pensive: return "Pensive";
        case Mood::Angry:   return "Angry";
        case Mood::Serene:  return "Serene";
        case Mood::Playful: return "Playful";
    }
    return "Neutral";
}

Mood moodFromString (const juce::String& s)
{
    for (auto m : { Mood::Neutral, Mood::Focused, Mood::Pensive, Mood::Angry, Mood::Serene, Mood::Playful })
        if (s.equalsIgnoreCase (moodToString (m)))
            return m;
    return Mood::Pensive;
}

juce::String laneName (int lane)
{
    switch (lane)
    {
        case laneVolume: return "Volume";
        case laneFilter: return "Filter";
        case lanePan:    return "Pan";
        default:         return {};
    }
}

float laneDefaultValue (int lane)
{
    return lane == lanePan ? 0.5f : 1.0f;
}

//==============================================================================
float Lane::shape (float t, float curve)
{
    if (std::abs (curve) < 1.0e-3f)
        return t;
    // curve > 0 : départ lent puis accélération ; curve < 0 : l'inverse.
    const float e = std::pow (2.0f, curve * 3.0f);
    return std::pow (juce::jlimit (0.0f, 1.0f, t), e);
}

float Lane::evaluate (float x) const
{
    if (points.empty())
        return 1.0f;
    if (x <= points.front().x)
        return points.front().y;
    if (x >= points.back().x)
        return points.back().y;

    auto it = std::upper_bound (points.begin(), points.end(), x,
                                [] (float v, const CurvePoint& p) { return v < p.x; });
    const auto& p1 = *it;
    const auto& p0 = *(it - 1);
    const float span = p1.x - p0.x;
    if (span <= 1.0e-7f)
        return p1.y;

    const float t = shape ((x - p0.x) / span, p0.curve);
    return p0.y + (p1.y - p0.y) * t;
}

void Lane::sanitize (float defaultValue)
{
    for (auto& p : points)
    {
        p.x = juce::jlimit (0.0f, 1.0f, p.x);
        p.y = juce::jlimit (0.0f, 1.0f, p.y);
        p.curve = juce::jlimit (-1.0f, 1.0f, p.curve);
    }

    std::stable_sort (points.begin(), points.end(),
                      [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

    if (points.empty())
        points.push_back ({ 0.0f, defaultValue, 0.0f });

    if (points.front().x > 0.0f)
    {
        const CurvePoint first { 0.0f, points.front().y, 0.0f };
        points.insert (points.begin(), first);
    }

    if (points.back().x < 1.0f)
    {
        const CurvePoint last { 1.0f, points.back().y, 0.0f };
        points.push_back (last);
    }
}

//==============================================================================
static juce::String lanePointsToString (const Lane& lane)
{
    juce::StringArray parts;
    for (const auto& p : lane.points)
        parts.add (juce::String (p.x, 5) + "," + juce::String (p.y, 4) + "," + juce::String (p.curve, 3));
    return parts.joinIntoString (";");
}

static Lane laneFromString (const juce::String& s, float def)
{
    Lane lane;
    for (const auto& item : juce::StringArray::fromTokens (s, ";", {}))
    {
        auto v = juce::StringArray::fromTokens (item, ",", {});
        if (v.size() >= 2)
            lane.points.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(), v.size() > 2 ? v[2].getFloatValue() : 0.0f });
    }
    lane.sanitize (def);
    return lane;
}

juce::ValueTree Preset::toValueTree() const
{
    juce::ValueTree v ("Preset");
    v.setProperty ("id", id, nullptr);
    v.setProperty ("name", name, nullptr);
    v.setProperty ("note", note, nullptr);
    v.setProperty ("archetype", archetype, nullptr);
    v.setProperty ("mood", moodToString (mood), nullptr);
    v.setProperty ("lengthBeats", lengthBeats, nullptr);
    v.setProperty ("seed", (juce::int64) seed, nullptr);
    v.setProperty ("saved", saved, nullptr);
    v.setProperty ("vol",    lanePointsToString (lanes[laneVolume]), nullptr);
    v.setProperty ("filter", lanePointsToString (lanes[laneFilter]), nullptr);
    v.setProperty ("pan",    lanePointsToString (lanes[lanePan]),    nullptr);
    return v;
}

Preset Preset::fromValueTree (const juce::ValueTree& v)
{
    Preset p;
    p.id          = v.getProperty ("id", juce::Uuid().toString());
    p.name        = v.getProperty ("name", "Untitled");
    p.note        = v.getProperty ("note");
    p.archetype   = v.getProperty ("archetype");
    p.mood        = moodFromString (v.getProperty ("mood"));
    p.lengthBeats = juce::jlimit (1.0, 32.0, (double) v.getProperty ("lengthBeats", 4.0));
    p.seed        = (juce::uint32) (juce::int64) v.getProperty ("seed", 0);
    p.saved       = v.getProperty ("saved", false);
    p.lanes[laneVolume] = laneFromString (v.getProperty ("vol"),    laneDefaultValue (laneVolume));
    p.lanes[laneFilter] = laneFromString (v.getProperty ("filter"), laneDefaultValue (laneFilter));
    p.lanes[lanePan]    = laneFromString (v.getProperty ("pan"),    laneDefaultValue (lanePan));
    return p;
}

Preset Preset::makeFlat()
{
    Preset p;
    p.id = juce::Uuid().toString();
    p.name = "Tabula Rasa";
    p.mood = Mood::Serene;
    for (int l = 0; l < numLanes; ++l)
        p.lanes[(size_t) l].sanitize (laneDefaultValue (l));
    return p;
}
