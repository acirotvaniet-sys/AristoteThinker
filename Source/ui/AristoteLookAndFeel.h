#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Palette noir & blanc « gravure sur papier ».
namespace Ink
{
    const juce::Colour paper  { 0xfff3f1ec };
    const juce::Colour ink    { 0xff111111 };
    const juce::Colour mid    { 0xff8a8984 };
    const juce::Colour faint  { 0xffd8d5ce };
    const juce::Colour ghost  { 0x14111111 };
}

namespace Type
{
    juce::Font sans (float size, bool bold = false);
    juce::Font caps (float size);           // petites capitales espacées
    juce::Font serif (float size, bool italic = false);
}

class AristoteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AristoteLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getAlertWindowMessageFont() override;
};
