#include "FxTab.h"
#include "PluginProcessor.h"

FxTab::FxTab (AristoteProcessor& p)
    : sat      (p.apvts, ParamIDs::satDrive,      "Drive"),
      dist     (p.apvts, ParamIDs::distAmount,    "Amount"),
      delayMix (p.apvts, ParamIDs::delayMix,      "Dry / Wet"),
      delayFb  (p.apvts, ParamIDs::delayFeedback, "Feedback"),
      cutoff   (p.apvts, ParamIDs::filterCutoff,  "Cutoff"),
      res      (p.apvts, ParamIDs::filterRes,     "Resonance"),
      eqAmount (p.apvts, ParamIDs::eqAmount,      "Amount"),
      output   (p.apvts, ParamIDs::outputGain,    "Gain"),
      delayTime  (p.apvts, ParamIDs::delayTime,  "Time"),
      filterType (p.apvts, ParamIDs::filterType, "Type"),
      eqPreset   (p.apvts, ParamIDs::eqPreset,   "Preset")
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &sat, &dist, &delayMix, &delayFb, &cutoff, &res,
                                                                        &eqAmount, &output, &delayTime, &filterType, &eqPreset })
        addAndMakeVisible (c);
}

void FxTab::paint (juce::Graphics& g)
{
    for (const auto& s : sections)
    {
        auto r = s.bounds.toFloat();
        g.setColour (Ink::ink);
        g.drawRect (r, 1.0f);

        // Titre posé sur le cadre
        g.setFont (Type::caps (11.0f));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s.title) + 16.0f;
        auto titleArea = juce::Rectangle<float> (r.getX() + 14.0f, r.getY() - 8.0f, tw, 16.0f);
        g.setColour (Ink::paper);
        g.fillRect (titleArea);
        g.setColour (Ink::ink);
        g.drawText (s.title, titleArea, juce::Justification::centred);
    }
}

void FxTab::resized()
{
    sections.clear();
    auto r = getLocalBounds().reduced (24, 24);
    const int gap = 18;
    const int rowH = (r.getHeight() - gap) / 2;

    auto row1 = r.removeFromTop (rowH);
    r.removeFromTop (gap);
    auto row2 = r;

    auto place = [this] (juce::Rectangle<int> area, const juce::String& title, std::initializer_list<juce::Component*> items)
    {
        sections.push_back ({ title, area });
        auto inner = area.reduced (16, 24);
        const int w = inner.getWidth() / (int) items.size();
        for (auto* c : items)
        {
            auto cell = inner.removeFromLeft (w).reduced (6, 0);
            const int h = juce::jmin (cell.getHeight(), 150);
            if (dynamic_cast<Choice*> (c) != nullptr)
                c->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 48));
            else
                c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 120), h));
        }
    };

    const int unit = (row1.getWidth() - 2 * gap) / 6;
    place (row1.removeFromLeft (unit), "SATURATION", { &sat });
    row1.removeFromLeft (gap);
    place (row1.removeFromLeft (unit), "DISTORTION", { &dist });
    row1.removeFromLeft (gap);
    place (row1, "DELAY", { &delayMix, &delayTime, &delayFb });

    const int unit2 = (row2.getWidth() - 2 * gap) / 7;
    place (row2.removeFromLeft (unit2 * 3), "FILTER", { &filterType, &cutoff, &res });
    row2.removeFromLeft (gap);
    place (row2.removeFromLeft (unit2 * 3), "EQ", { &eqPreset, &eqAmount });
    row2.removeFromLeft (gap);
    place (row2, "OUTPUT", { &output });
}
