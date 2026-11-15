#include "AristoteLookAndFeel.h"

namespace Type
{
    juce::Font sans (float size, bool bold)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font caps (float size)
    {
        return juce::Font (juce::FontOptions (size)).withExtraKerningFactor (0.18f);
    }

    juce::Font serif (float size, bool italic)
    {
        return juce::Font (juce::FontOptions ("Georgia", size, italic ? juce::Font::italic : juce::Font::plain));
    }
}

AristoteLookAndFeel::AristoteLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Ink::paper);
    setColour (TextButton::buttonColourId, Ink::paper);
    setColour (TextButton::buttonOnColourId, Ink::ink);
    setColour (TextButton::textColourOffId, Ink::ink);
    setColour (TextButton::textColourOnId, Ink::paper);
    setColour (ComboBox::backgroundColourId, Ink::paper);
    setColour (ComboBox::textColourId, Ink::ink);
    setColour (ComboBox::outlineColourId, Ink::ink);
    setColour (ComboBox::arrowColourId, Ink::ink);
    setColour (PopupMenu::backgroundColourId, Ink::paper);
    setColour (PopupMenu::textColourId, Ink::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, Ink::ink);
    setColour (PopupMenu::highlightedTextColourId, Ink::paper);
    setColour (Label::textColourId, Ink::ink);
    setColour (Slider::textBoxTextColourId, Ink::ink);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Ink::faint);
    setColour (ListBox::backgroundColourId, Ink::paper);
    setColour (ListBox::outlineColourId, Ink::ink);
    setColour (TextEditor::backgroundColourId, Ink::paper);
    setColour (TextEditor::textColourId, Ink::ink);
    setColour (TextEditor::outlineColourId, Ink::ink);
    setColour (TextEditor::focusedOutlineColourId, Ink::ink);
    setColour (TextEditor::highlightColourId, Ink::faint);
    setColour (CaretComponent::caretColourId, Ink::ink);
    setColour (AlertWindow::backgroundColourId, Ink::paper);
    setColour (AlertWindow::textColourId, Ink::ink);
    setColour (AlertWindow::outlineColourId, Ink::ink);
    setColour (ToggleButton::textColourId, Ink::ink);
    setColour (ToggleButton::tickColourId, Ink::ink);
    setColour (ScrollBar::thumbColourId, Ink::mid);
    setColour (TooltipWindow::backgroundColourId, Ink::ink);
    setColour (TooltipWindow::textColourId, Ink::paper);
}

void AristoteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Piste fine
    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (Ink::faint);
    g.strokePath (track, PathStrokeType (1.0f));

    // Valeur
    Path value;
    value.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, angle, true);
    g.setColour (Ink::ink);
    g.strokePath (value, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::butt));

    // Disque intérieur + aiguille
    const float ir = r * 0.62f;
    g.setColour (Ink::paper);
    g.fillEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f);
    g.setColour (Ink::ink);
    g.drawEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f, 1.0f);
    const auto tip = c.getPointOnCircumference (ir * 0.85f, angle);
    g.drawLine ({ c.getPointOnCircumference (ir * 0.25f, angle), tip }, 1.6f);
}

void AristoteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on || down ? Ink::ink : (highlighted ? Ink::faint.withAlpha (0.6f) : Ink::paper));
    g.fillRect (r);
    g.setColour (Ink::ink.withAlpha (b.isEnabled() ? 1.0f : 0.3f));
    g.drawRect (r, 1.0f);
}

void AristoteLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    g.setColour ((on ? Ink::paper : Ink::ink).withAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.setFont (Type::caps (11.0f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void AristoteLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.25f);
    g.setColour (Ink::ink);
    g.drawRect (box, 1.0f);
    if (b.getToggleState())
        g.fillRect (box.reduced (3.0f));
    g.setFont (Type::caps (11.0f));
    g.drawFittedText (b.getButtonText().toUpperCase(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

void AristoteLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    g.setColour (Ink::ink);
    g.drawLine (0.0f, (float) h - 0.5f, (float) w, (float) h - 0.5f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.fillPath (arrow);
}

juce::Font AristoteLookAndFeel::getComboBoxFont (juce::ComboBox&)   { return Type::caps (11.0f); }
juce::Font AristoteLookAndFeel::getPopupMenuFont()                  { return Type::sans (13.0f); }
juce::Font AristoteLookAndFeel::getLabelFont (juce::Label& l)       { return l.getFont(); }
juce::Font AristoteLookAndFeel::getAlertWindowMessageFont()         { return Type::sans (14.0f); }

void AristoteLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (0, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}
