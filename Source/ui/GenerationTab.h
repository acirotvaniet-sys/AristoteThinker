#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AristotleHead.h"
#include "CurveEditor.h"
#include "Knob.h"

class AristoteProcessor;

// Onglet 1 : analyse, presets générés, tête d'Aristote, éditeur de courbes.
class GenerationTab : public juce::Component,
                      private juce::ChangeListener,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    explicit GenerationTab (AristoteProcessor& p);
    ~GenerationTab() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ChangeListener
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;
    void refreshAll();
    void updateMood();
    void showRename (int row);
    void setLane (int lane);

    AristoteProcessor& proc;
    std::vector<Preset> presets;
    bool updating = false;

    juce::ListBox list { "Presets", this };
    juce::TextButton saveBtn { "Save" }, renameBtn { "Rename" }, deleteBtn { "Delete" },
                     regenBtn { "Regen" }, loadBtn { "Load" };

    AristotleHead head;
    juce::TextButton analyzeBtn { "Analyze" };
    Choice barsChoice;
    juce::Label statusLabel, analysisLabel, noteLabel, moodLabel;

    juce::TextButton laneBtns[3];
    juce::ComboBox gridBox;
    Knob volDepth, filterDepth, panDepth, smooth;
    CurveEditor curve;

    juce::Rectangle<float> indicatorArea;
    float pulse = 0.0f;

    std::unique_ptr<juce::AlertWindow> renameWindow;
    std::unique_ptr<juce::FileChooser> chooser;
};
