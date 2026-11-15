#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "AppSettings.h"
#include "model/Preset.h"
#include "analysis/ChordAnalyzer.h"
#include "modulation/ModulationEngine.h"
#include "fx/FxChain.h"

class AristoteProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::Timer
{
public:
    AristoteProcessor();
    ~AristoteProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Gestion des presets (thread message uniquement)
    std::vector<Preset> getPresets() const;
    int getActiveIndex() const;
    Preset getActivePreset() const;

    void selectPreset (int index);
    void renamePreset (int index, const juce::String& newName);
    void deletePreset (int index);
    void regeneratePreset (int index);
    bool savePreset (int index);                       // garde le preset + écrit un fichier
    void updateActivePreset (const Preset& edited);   // édition des courbes à la main
    int importPresetFile (const juce::File& file);

    //==========================================================================
    // Analyse
    enum class Stage { Idle, Armed, Listening, Analyzing, Thinking };
    void startAnalysis();
    void cancelAnalysis();
    Stage getStage() const;
    float getListenProgress() const;
    AnalysisResult getLastAnalysis() const;
    juce::String getStatusText() const;
    double getHostBpm() const { return hostBpm.load(); }

    ModulationEngine& getModulationEngine() { return modEngine; }
    AppSettings& getSettings() { return settings; }

    juce::AudioProcessorValueTreeState apvts;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AristoteProcessor)

private:
    void timerCallback() override;
    void pushActiveTables();
    void onAnalysisDone (AnalysisResult result, std::vector<Preset> fresh);
    void askClaude (std::vector<Preset> fresh);
    void setStatus (const juce::String& s);
    void captureAudio (const juce::AudioBuffer<float>& buffer, double ppqStart, double bpm, bool playing, int beatsPerBar);

    AppSettings settings;
    ModulationEngine modEngine;
    FxChain fx;

    // --- presets ---
    mutable juce::CriticalSection presetLock;
    std::vector<Preset> presets;
    int activeIndex = 0;
    AnalysisResult lastAnalysis;
    juce::String status;
    Stage uiStage = Stage::Idle;

    // --- capture audio (thread audio <-> thread message) ---
    enum CaptureState { capIdle = 0, capArmed, capCapturing, capDone };
    std::atomic<int> captureState { capIdle };
    std::atomic<int> captureTargetBeats { 16 };
    std::vector<float> captureBuffer;
    std::atomic<int> captureWritten { 0 };
    std::atomic<int> captureTarget { 0 };
    std::atomic<double> captureBpm { 120.0 };
    std::atomic<int> captureBeatsPerBar { 4 };

    // --- transport ---
    double currentSampleRate = 44100.0;
    double internalPpq = 0.0;
    std::atomic<double> hostBpm { 120.0 };

    juce::ThreadPool pool { 1 };
    juce::uint32 generationCounter = 1;

    std::atomic<float>* pVolDepth = nullptr;
    std::atomic<float>* pFilterDepth = nullptr;
    std::atomic<float>* pPanDepth = nullptr;
    std::atomic<float>* pSmoothing = nullptr;
    std::atomic<float>* pAnalysisBars = nullptr;
    std::atomic<float>* pSat = nullptr;
    std::atomic<float>* pDist = nullptr;
    std::atomic<float>* pDelayMix = nullptr;
    std::atomic<float>* pDelayTime = nullptr;
    std::atomic<float>* pDelayFb = nullptr;
    std::atomic<float>* pFilterType = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pRes = nullptr;
    std::atomic<float>* pEqPreset = nullptr;
    std::atomic<float>* pEqAmount = nullptr;
    std::atomic<float>* pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AristoteProcessor)
};
