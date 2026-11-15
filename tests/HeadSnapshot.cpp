// Rend les 6 expressions d'Aristote en PNG (docs/expressions.png).
#include "ui/AristotleHead.h"
#include "ui/AristoteLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    AristoteLookAndFeel lnf;
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    const Mood moods[] { Mood::Neutral, Mood::Focused, Mood::Pensive, Mood::Angry, Mood::Serene, Mood::Playful };
    const int cw = 300, ch = 360;
    juce::Image sheet (juce::Image::RGB, cw * 3, ch * 2, true);
    {
        juce::Graphics g (sheet);
        g.fillAll (Ink::paper);
        for (int i = 0; i < 6; ++i)
        {
            AristotleHead head;
            head.setSize (cw - 20, ch - 50);
            head.setMoodImmediately (moods[i]);
            auto img = head.createComponentSnapshot (head.getLocalBounds(), true, 1.0f);
            const int x = (i % 3) * cw, y = (i / 3) * ch;
            g.drawImageAt (img, x + 10, y + 10);
            g.setColour (Ink::ink);
            g.setFont (Type::caps (13.0f));
            g.drawText (moodToString (moods[i]).toUpperCase(), x, y + ch - 36, cw, 24, juce::Justification::centred);
        }
    }

    auto out = juce::File (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("expressions.png").getFullPathName());
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (sheet, stream);
    std::cout << (ok ? "wrote " : "FAILED ") << out.getFullPathName() << "\n";
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    return ok ? 0 : 1;
}
