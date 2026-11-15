#include "PluginEditor.h"

namespace
{
    // Petit panneau « AI » : clé API Claude + activation.
    class AiPanel : public juce::Component
    {
    public:
        explicit AiPanel (AppSettings& s) : settings (s)
        {
            title.setText ("CLAUDE NAMING", juce::dontSendNotification);
            title.setFont (Type::caps (11.0f));
            addAndMakeVisible (title);

            info.setText ("After each analysis, Claude renames the presets and writes a producer note. "
                          "Needs an Anthropic API key and internet. The key is stored in plain text in your "
                          "user settings folder, not in your DAW project.", juce::dontSendNotification);
            info.setFont (Type::sans (12.0f));
            info.setColour (juce::Label::textColourId, Ink::mid);
            info.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (info);

            key.setPasswordCharacter ((juce::juce_wchar) 0x2022);
            key.setTextToShowWhenEmpty ("sk-ant-...", Ink::mid);
            key.setText (settings.getApiKey(), false);
            key.onTextChange = [this] { settings.setApiKey (key.getText()); };
            addAndMakeVisible (key);

            enable.setButtonText ("Use Claude after analysis");
            enable.setToggleState (settings.isClaudeEnabled(), juce::dontSendNotification);
            enable.onClick = [this] { settings.setClaudeEnabled (enable.getToggleState()); };
            addAndMakeVisible (enable);

            setSize (320, 190);
        }

        void paint (juce::Graphics& g) override { g.fillAll (Ink::paper); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            title.setBounds (r.removeFromTop (18));
            info.setBounds (r.removeFromTop (70));
            r.removeFromTop (6);
            key.setBounds (r.removeFromTop (26));
            r.removeFromTop (10);
            enable.setBounds (r.removeFromTop (24));
        }

    private:
        AppSettings& settings;
        juce::Label title, info;
        juce::TextEditor key;
        juce::ToggleButton enable;
    };
}

AristoteEditor::AristoteEditor (AristoteProcessor& p)
    : AudioProcessorEditor (p), proc (p), genTab (p), fxTab (p)
{
    setLookAndFeel (&lnf);

    for (auto* b : { &genTabBtn, &fxTabBtn })
    {
        b->setRadioGroupId (2001);
        addAndMakeVisible (b);
    }
    genTabBtn.onClick = [this] { showTab (0); };
    fxTabBtn.onClick  = [this] { showTab (1); };

    aiBtn.setTooltip ("Claude naming settings");
    aiBtn.onClick = [this] { showAiSettings(); };
    addAndMakeVisible (aiBtn);

    addChildComponent (genTab);
    addChildComponent (fxTab);
    showTab (0);

    setSize (980, 690);
}

AristoteEditor::~AristoteEditor()
{
    setLookAndFeel (nullptr);
}

void AristoteEditor::showTab (int index)
{
    genTab.setVisible (index == 0);
    fxTab.setVisible (index == 1);
    genTabBtn.setToggleState (index == 0, juce::dontSendNotification);
    fxTabBtn.setToggleState (index == 1, juce::dontSendNotification);
}

void AristoteEditor::showAiSettings()
{
    auto panel = std::make_unique<AiPanel> (proc.getSettings());
    panel->setLookAndFeel (&lnf);
    // Parent = l'éditeur : la bulle est détruite avec lui (sûr dans un hôte).
    juce::CallOutBox::launchAsynchronously (std::move (panel), aiBtn.getBounds(), this);
}

void AristoteEditor::paint (juce::Graphics& g)
{
    g.fillAll (Ink::paper);

    auto header = getLocalBounds().removeFromTop (60).reduced (20, 0);
    g.setColour (Ink::ink);
    g.setFont (Type::serif (26.0f));
    g.drawText ("AristoteThinker", header.removeFromLeft (230), juce::Justification::centredLeft);
    g.setColour (Ink::mid);
    g.setFont (Type::caps (10.0f));
    g.drawText ("RHYTHMIC REASONING", header.removeFromLeft (200), juce::Justification::centredLeft);

    g.setColour (Ink::ink);
    g.drawHorizontalLine (60, 20.0f, (float) getWidth() - 20.0f);
}

void AristoteEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (60).reduced (20, 14);
    aiBtn.setBounds (header.removeFromRight (44));
    header.removeFromRight (16);
    fxTabBtn.setBounds (header.removeFromRight (110));
    genTabBtn.setBounds (header.removeFromRight (130));

    r.removeFromTop (8);
    genTab.setBounds (r);
    fxTab.setBounds (r);
}
