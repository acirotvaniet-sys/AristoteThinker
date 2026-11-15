#include "GenerationTab.h"
#include "PluginProcessor.h"
#include <cmath>

GenerationTab::GenerationTab (AristoteProcessor& p)
    : proc (p),
      barsChoice (p.apvts, ParamIDs::analysisBars, "Listen for"),
      volDepth (p.apvts, ParamIDs::volDepth, "Volume"),
      filterDepth (p.apvts, ParamIDs::filterDepth, "Filter"),
      panDepth (p.apvts, ParamIDs::panDepth, "Pan"),
      smooth (p.apvts, ParamIDs::smoothing, "Smooth"),
      curve (p)
{
    list.setRowHeight (30);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    for (auto* b : { &saveBtn, &renameBtn, &deleteBtn, &regenBtn, &loadBtn })
        addAndMakeVisible (b);

    saveBtn.setTooltip ("Keep this preset and write it to Documents/AristoteThinker/Presets");
    regenBtn.setTooltip ("Regenerate this preset with the same pattern family");

    saveBtn.onClick = [this]
    {
        const int r = list.getSelectedRow();
        if (r >= 0 && ! proc.savePreset (r))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed",
                                                    "Could not write the preset file.");
    };
    renameBtn.onClick = [this] { showRename (list.getSelectedRow()); };
    deleteBtn.onClick = [this] { proc.deletePreset (list.getSelectedRow()); };
    regenBtn.onClick  = [this] { proc.regeneratePreset (list.getSelectedRow()); };
    loadBtn.onClick   = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a preset", AppSettings::getPresetFolder(), "*.atp");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                      proc.importPresetFile (f);
                              });
    };

    addAndMakeVisible (head);

    analyzeBtn.onClick = [this]
    {
        const auto st = proc.getStage();
        if (st == AristoteProcessor::Stage::Armed || st == AristoteProcessor::Stage::Listening)
            proc.cancelAnalysis();
        else
            proc.startAnalysis();
    };
    addAndMakeVisible (analyzeBtn);
    addAndMakeVisible (barsChoice);

    statusLabel.setFont (Type::sans (13.0f));
    statusLabel.setColour (juce::Label::textColourId, Ink::mid);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    analysisLabel.setFont (Type::caps (11.0f));
    analysisLabel.setJustificationType (juce::Justification::centred);
    noteLabel.setFont (Type::serif (14.0f, true));
    noteLabel.setJustificationType (juce::Justification::topLeft);
    moodLabel.setFont (Type::caps (13.0f));
    moodLabel.setJustificationType (juce::Justification::centred);
    for (auto* l : { &statusLabel, &analysisLabel, &noteLabel, &moodLabel })
        addAndMakeVisible (l);

    const char* laneTitles[] { "Volume", "Filter", "Pan" };
    for (int i = 0; i < 3; ++i)
    {
        laneBtns[i].setButtonText (laneTitles[i]);
        laneBtns[i].setRadioGroupId (1001);
        laneBtns[i].onClick = [this, i] { setLane (i); };
        addAndMakeVisible (laneBtns[i]);
    }

    gridBox.addItemList (CurveEditor::gridNames(), 1);
    gridBox.setSelectedId (2, juce::dontSendNotification);
    gridBox.onChange = [this] { curve.setGridIndex (gridBox.getSelectedId() - 1); };
    gridBox.setTooltip ("Grid");
    addAndMakeVisible (gridBox);

    for (auto* k : { &volDepth, &filterDepth, &panDepth, &smooth })
        addAndMakeVisible (k);

    addAndMakeVisible (curve);

    proc.addChangeListener (this);
    refreshAll();
    setLane (laneVolume);
    startTimerHz (30);
}

GenerationTab::~GenerationTab()
{
    proc.removeChangeListener (this);
}

//==============================================================================
void GenerationTab::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void GenerationTab::refreshAll()
{
    updating = true;
    presets = proc.getPresets();
    list.updateContent();
    const int active = proc.getActiveIndex();
    list.selectRow (active);
    list.repaint();
    updating = false;

    analysisLabel.setText (proc.getLastAnalysis().summary(), juce::dontSendNotification);
    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);
    if (active >= 0 && active < (int) presets.size())
        noteLabel.setText (presets[(size_t) active].note, juce::dontSendNotification);

    curve.refresh();
    updateMood();
}

void GenerationTab::updateMood()
{
    const auto st = proc.getStage();
    Mood m = Mood::Neutral;
    if (st != AristoteProcessor::Stage::Idle)
        m = Mood::Focused;
    else
    {
        const int a = proc.getActiveIndex();
        if (a >= 0 && a < (int) presets.size())
            m = presets[(size_t) a].mood;
    }

    if (m != head.getMood())
        head.setMood (m);
    moodLabel.setText (moodToString (m).toUpperCase(), juce::dontSendNotification);
}

void GenerationTab::timerCallback()
{
    const auto st = proc.getStage();
    using S = AristoteProcessor::Stage;

    const bool busy = st == S::Analyzing || st == S::Thinking;
    analyzeBtn.setButtonText (st == S::Armed || st == S::Listening ? "Cancel" : busy ? "Thinking..." : "Analyze");
    analyzeBtn.setEnabled (! busy);
    analyzeBtn.setToggleState (st == S::Armed || st == S::Listening, juce::dontSendNotification);
    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);

    pulse += 0.12f;
    updateMood();
    repaint (indicatorArea.expanded (4.0f).toNearestInt());
}

void GenerationTab::setLane (int l)
{
    for (int i = 0; i < 3; ++i)
        laneBtns[i].setToggleState (i == l, juce::dontSendNotification);
    curve.setLane (l);
}

void GenerationTab::showRename (int row)
{
    if (row < 0 || row >= (int) presets.size())
        return;

    renameWindow = std::make_unique<juce::AlertWindow> ("Rename preset", "", juce::MessageBoxIconType::NoIcon, this);
    renameWindow->setLookAndFeel (&getLookAndFeel());
    renameWindow->addTextEditor ("name", presets[(size_t) row].name);
    renameWindow->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    renameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<GenerationTab> safe (this);
    renameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe, row] (int result)
    {
        auto* t = safe.getComponent();
        if (t == nullptr || t->renameWindow == nullptr)
            return;
        if (result == 1)
            t->proc.renamePreset (row, t->renameWindow->getTextEditorContents ("name"));
        juce::MessageManager::callAsync ([safe]
        {
            if (auto* t2 = safe.getComponent())
                t2->renameWindow.reset();
        });
    }), false);
}

//==============================================================================
int GenerationTab::getNumRows()
{
    return (int) presets.size();
}

void GenerationTab::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) presets.size())
        return;
    const auto& p = presets[(size_t) row];

    g.fillAll (selected ? Ink::ink : Ink::paper);
    const auto fg = selected ? Ink::paper : Ink::ink;

    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (10, 0);
    g.setColour (fg.withAlpha (0.6f));
    g.setFont (Type::caps (9.0f));
    g.drawText (moodToString (p.mood).toUpperCase(), r.removeFromRight (62), juce::Justification::centredRight);

    if (p.saved)
    {
        g.setColour (fg);
        g.fillEllipse (r.removeFromRight (12).toFloat().withSizeKeepingCentre (5.0f, 5.0f));
    }

    g.setColour (fg);
    g.setFont (Type::sans (14.0f));
    g.drawText (p.name, r, juce::Justification::centredLeft, true);

    g.setColour (Ink::faint);
    g.drawHorizontalLine (h - 1, 0.0f, (float) w);
}

void GenerationTab::selectedRowsChanged (int row)
{
    if (updating)
        return;
    if (row >= 0 && row != proc.getActiveIndex())
        proc.selectPreset (row);
}

void GenerationTab::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    showRename (row);
}

//==============================================================================
void GenerationTab::paint (juce::Graphics& g)
{
    g.setColour (Ink::mid);
    g.setFont (Type::caps (10.0f));
    g.drawText ("PRESETS", list.getBounds().translated (0, -18).withHeight (16), juce::Justification::centredLeft);
    g.drawText ("ANALYSIS", analyzeBtn.getBounds().translated (0, -18).withHeight (16), juce::Justification::centredLeft);

    // Ligne de séparation au-dessus de l'éditeur de courbes
    g.setColour (Ink::faint);
    g.drawHorizontalLine (laneBtns[0].getY() - 12, 16.0f, (float) getWidth() - 16.0f);

    // Indicateur d'écoute : anneau de progression + point qui pulse
    const auto st = proc.getStage();
    using S = AristoteProcessor::Stage;
    const float d = juce::jmin (indicatorArea.getHeight(), 44.0f);
    const auto c = juce::Rectangle<float> (d, d).withCentre ({ indicatorArea.getX() + d * 0.5f, indicatorArea.getCentreY() });

    g.setColour (Ink::faint);
    g.drawEllipse (c, 1.0f);

    const float prog = proc.getListenProgress();
    if (prog > 0.0f)
    {
        juce::Path arc;
        arc.addCentredArc (c.getCentreX(), c.getCentreY(), d * 0.5f, d * 0.5f, 0.0f, 0.0f,
                           prog * juce::MathConstants<float>::twoPi, true);
        g.setColour (Ink::ink);
        g.strokePath (arc, juce::PathStrokeType (2.5f));
    }

    float dot = 0.18f;
    if (st == S::Listening)                       dot = 0.25f + 0.08f * std::sin (pulse * 2.0f);
    else if (st == S::Armed)                      dot = 0.18f + 0.06f * std::sin (pulse * 4.0f);
    else if (st == S::Analyzing || st == S::Thinking) dot = 0.22f + 0.05f * std::sin (pulse * 6.0f);
    g.setColour (st == S::Idle ? Ink::faint : Ink::ink);
    g.fillEllipse (c.withSizeKeepingCentre (d * dot * 2.0f, d * dot * 2.0f));
}

void GenerationTab::resized()
{
    auto r = getLocalBounds().reduced (16, 8);
    auto bottom = r.removeFromBottom (230);
    auto top = r;

    // --- Colonne gauche : presets ---
    auto left = top.removeFromLeft (240);
    left.removeFromTop (22);
    auto buttons = left.removeFromBottom (62);
    list.setBounds (left.withTrimmedBottom (8));
    {
        auto row1 = buttons.removeFromTop (28);
        const int w = row1.getWidth() / 3;
        saveBtn.setBounds (row1.removeFromLeft (w).reduced (2, 0));
        renameBtn.setBounds (row1.removeFromLeft (w).reduced (2, 0));
        loadBtn.setBounds (row1.reduced (2, 0));
        buttons.removeFromTop (6);
        auto row2 = buttons.removeFromTop (28);
        const int w2 = row2.getWidth() / 2;
        deleteBtn.setBounds (row2.removeFromLeft (w2).reduced (2, 0));
        regenBtn.setBounds (row2.reduced (2, 0));
    }
    top.removeFromLeft (16);

    // --- Colonne droite : analyse ---
    auto right = top.removeFromRight (240);
    right.removeFromTop (22);
    analyzeBtn.setBounds (right.removeFromTop (44));
    right.removeFromTop (10);
    barsChoice.setBounds (right.removeFromTop (44));
    right.removeFromTop (14);
    auto ind = right.removeFromTop (52);
    indicatorArea = ind.toFloat();
    statusLabel.setBounds (ind.withTrimmedLeft (56));
    right.removeFromTop (12);
    noteLabel.setBounds (right);
    top.removeFromRight (16);

    // --- Centre : Aristote ---
    moodLabel.setBounds (top.removeFromBottom (20));
    analysisLabel.setBounds (top.removeFromBottom (20));
    head.setBounds (top.reduced (0, 4));

    // --- Bas : éditeur de courbes ---
    bottom.removeFromTop (16);
    auto controls = bottom.removeFromTop (62);
    {
        auto knobs = controls.removeFromRight (4 * 72);
        for (auto* k : { &volDepth, &filterDepth, &panDepth, &smooth })
            k->setBounds (knobs.removeFromLeft (72));

        auto row = controls.withSizeKeepingCentre (controls.getWidth(), 26).withY (controls.getY() + 18);
        for (int i = 0; i < 3; ++i)
            laneBtns[i].setBounds (row.removeFromLeft (84).reduced (2, 0));
        row.removeFromLeft (14);
        gridBox.setBounds (row.removeFromLeft (90));
    }
    curve.setBounds (bottom);
}
