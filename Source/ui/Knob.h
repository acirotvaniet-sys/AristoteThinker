#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AristoteLookAndFeel.h"

// Potard + libellé, relié à un paramètre de l'APVTS.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
        addAndMakeVisible (slider);

        label.setText (title.toUpperCase(), juce::dontSendNotification);
        label.setFont (Type::caps (10.0f));
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Sélecteur (ComboBox) + libellé, relié à un paramètre « choice ».
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);

        label.setText (title.toUpperCase(), juce::dontSendNotification);
        label.setFont (Type::caps (10.0f));
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        box.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (26, r.getHeight())));
    }

    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
