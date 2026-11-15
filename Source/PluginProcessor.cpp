#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "generation/TrapGenerator.h"
#include "generation/ClaudeAdvisor.h"

AristoteProcessor::AristoteProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pVolDepth     = apvts.getRawParameterValue (ParamIDs::volDepth);
    pFilterDepth  = apvts.getRawParameterValue (ParamIDs::filterDepth);
    pPanDepth     = apvts.getRawParameterValue (ParamIDs::panDepth);
    pSmoothing    = apvts.getRawParameterValue (ParamIDs::smoothing);
    pAnalysisBars = apvts.getRawParameterValue (ParamIDs::analysisBars);
    pSat          = apvts.getRawParameterValue (ParamIDs::satDrive);
    pDist         = apvts.getRawParameterValue (ParamIDs::distAmount);
    pDelayMix     = apvts.getRawParameterValue (ParamIDs::delayMix);
    pDelayTime    = apvts.getRawParameterValue (ParamIDs::delayTime);
    pDelayFb      = apvts.getRawParameterValue (ParamIDs::delayFeedback);
    pFilterType   = apvts.getRawParameterValue (ParamIDs::filterType);
    pCutoff       = apvts.getRawParameterValue (ParamIDs::filterCutoff);
    pRes          = apvts.getRawParameterValue (ParamIDs::filterRes);
    pEqPreset     = apvts.getRawParameterValue (ParamIDs::eqPreset);
    pEqAmount     = apvts.getRawParameterValue (ParamIDs::eqAmount);
    pOut          = apvts.getRawParameterValue (ParamIDs::outputGain);

    // Version minimale : un preset généré par défaut, actif dès le chargement.
    presets.push_back (TrapGenerator::makeDefault());
    status = "Ready. Play your loop and press ANALYZE.";
    pushActiveTables();

    startTimerHz (30);
}

AristoteProcessor::~AristoteProcessor()
{
    stopTimer();
    pool.removeAllJobs (true, 4000);
}

//==============================================================================
void AristoteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    modEngine.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (fx.getLatencySamples());

    // Jusqu'à 64 s d'enregistrement mono (8 mesures à 30 BPM)
    captureState.store (capIdle);
    captureBuffer.assign ((size_t) (sampleRate * 64.0), 0.0f);
    pushActiveTables();
}

bool AristoteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void AristoteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, n);

    // --- Position tempo de l'hôte ---
    double bpm = hostBpm.load();
    double ppq = internalPpq;
    bool playing = false;
    int beatsPerBar = 4;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())            bpm = *b;
            if (auto ts = pos->getTimeSignature()) beatsPerBar = juce::jmax (1, ts->numerator);
            playing = pos->getIsPlaying();
            if (playing)
                if (auto p = pos->getPpqPosition())
                    ppq = *p;
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    hostBpm.store (bpm);

    captureAudio (buffer, ppq, bpm, playing, beatsPerBar);

    ModulationEngine::Settings ms;
    ms.volDepth    = pVolDepth->load();
    ms.filterDepth = pFilterDepth->load();
    ms.panDepth    = pPanDepth->load();
    ms.smoothingMs = pSmoothing->load();
    modEngine.process (buffer, n, ppq, bpm, ms);

    FxChain::Settings fs;
    fs.satDrive       = pSat->load();
    fs.distAmount     = pDist->load();
    fs.delayMix       = pDelayMix->load();
    fs.delayTimeIndex = (int) pDelayTime->load();
    fs.delayFeedback  = pDelayFb->load();
    fs.filterType     = (int) pFilterType->load();
    fs.cutoff         = pCutoff->load();
    fs.resonance      = pRes->load();
    fs.eqPreset       = (int) pEqPreset->load();
    fs.eqAmount       = pEqAmount->load();
    fs.outputDb       = pOut->load();
    fs.bpm            = bpm;
    fx.process (buffer, fs);

    // Horloge interne quand l'hôte est à l'arrêt (le motif continue de tourner)
    internalPpq = ppq + (double) n * bpm / 60.0 / currentSampleRate;
}

void AristoteProcessor::captureAudio (const juce::AudioBuffer<float>& buffer, double ppq, double bpm,
                                      bool playing, int beatsPerBar)
{
    int st = captureState.load();
    if (st != capArmed && st != capCapturing)
        return;

    const int n = buffer.getNumSamples();
    int offset = 0;

    if (st == capArmed)
    {
        if (playing)
        {
            // Démarre sur la prochaine barre de mesure pour rester calé sur la grille
            const double samplesPerBeat = currentSampleRate * 60.0 / bpm;
            const double barLen = (double) beatsPerBar;
            const double nextBar = std::ceil (ppq / barLen - 1.0e-9) * barLen;
            const double off = (nextBar - ppq) * samplesPerBeat;
            if (off >= (double) n)
                return;
            offset = juce::jlimit (0, n - 1, (int) off);
        }

        const int target = juce::jmin ((int) captureBuffer.size(),
                                       (int) (captureTargetBeats.load() * 60.0 / bpm * currentSampleRate));
        captureTarget.store (target);
        captureWritten.store (0);
        captureBpm.store (bpm);
        captureBeatsPerBar.store (beatsPerBar);
        captureState.store (capCapturing);
    }

    const int written = captureWritten.load();
    const int target = captureTarget.load();
    const int toCopy = juce::jmin (n - offset, target - written);
    const int numCh = buffer.getNumChannels();

    if (toCopy > 0 && numCh > 0)
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = numCh > 1 ? buffer.getReadPointer (1) : l;
        float* dst = captureBuffer.data() + written;
        for (int i = 0; i < toCopy; ++i)
            dst[i] = 0.5f * (l[offset + i] + r[offset + i]);
        captureWritten.store (written + toCopy);
    }

    if (written + juce::jmax (0, toCopy) >= target)
        captureState.store (capDone);
}

//==============================================================================
void AristoteProcessor::timerCallback()
{
    const int st = captureState.load();

    if (uiStage == Stage::Armed && st == capCapturing)
    {
        uiStage = Stage::Listening;
        setStatus ("Listening...");
    }

    if (st != capDone)
        return;

    captureState.store (capIdle);
    const int written = captureWritten.load();
    std::vector<float> data (captureBuffer.begin(), captureBuffer.begin() + written);
    const double sr = currentSampleRate;
    const double bpm = captureBpm.load();
    const int bpb = captureBeatsPerBar.load();
    const auto seed = (juce::uint32) juce::Time::getMillisecondCounter() ^ (generationCounter++ * 2654435761u);

    uiStage = Stage::Analyzing;
    setStatus ("Aristotle is thinking...");

    juce::WeakReference<AristoteProcessor> weak (this);
    pool.addJob ([weak, data, sr, bpm, bpb, seed]
    {
        auto result = ChordAnalyzer::analyze (data.data(), (int) data.size(), sr, bpm, bpb);
        auto fresh = TrapGenerator::generate (result, 4, seed);
        juce::MessageManager::callAsync ([weak, result, fresh]
        {
            if (auto* p = weak.get())
                p->onAnalysisDone (result, fresh);
        });
    });
}

void AristoteProcessor::onAnalysisDone (AnalysisResult result, std::vector<Preset> fresh)
{
    {
        const juce::ScopedLock sl (presetLock);
        lastAnalysis = result;

        // Les presets sauvegardés sont conservés ; les autres sont remplacés.
        std::vector<Preset> next = fresh;
        for (const auto& p : presets)
            if (p.saved)
                next.push_back (p);
        presets = std::move (next);
        activeIndex = 0;
    }
    pushActiveTables();

    if (settings.isClaudeEnabled() && settings.getApiKey().isNotEmpty() && ! result.silent)
    {
        askClaude (fresh);
    }
    else
    {
        uiStage = Stage::Idle;
        setStatus (result.silent ? "I heard only silence - here are some default ideas."
                                 : "Generated " + juce::String ((int) fresh.size()) + " presets from what I heard.");
    }
    sendChangeMessage();
}

void AristoteProcessor::askClaude (std::vector<Preset> fresh)
{
    uiStage = Stage::Thinking;
    setStatus ("Consulting Claude...");

    juce::WeakReference<AristoteProcessor> weak (this);
    const auto key = settings.getApiKey();
    const auto analysis = getLastAnalysis();

    pool.addJob ([weak, key, analysis, fresh]
    {
        std::vector<ClaudeAdvisor::Suggestion> suggestions;
        const auto res = ClaudeAdvisor::request (key, analysis, fresh, suggestions);

        juce::MessageManager::callAsync ([weak, res, suggestions]
        {
            auto* p = weak.get();
            if (p == nullptr)
                return;
            {
                const juce::ScopedLock sl (p->presetLock);
                for (const auto& s : suggestions)
                    for (auto& preset : p->presets)
                        if (preset.id == s.presetId && ! preset.saved)
                        {
                            preset.name = s.name;
                            if (s.note.isNotEmpty())
                                preset.note = s.note;
                        }
            }
            p->uiStage = Stage::Idle;
            p->setStatus (res.wasOk() ? juce::String ("Claude named the presets.")
                                      : "Claude unavailable (" + res.getErrorMessage() + ") - kept rule-based names.");
            p->sendChangeMessage();
        });
    });
}

void AristoteProcessor::setStatus (const juce::String& s)
{
    {
        const juce::ScopedLock sl (presetLock);
        status = s;
    }
    sendChangeMessage();
}

//==============================================================================
void AristoteProcessor::startAnalysis()
{
    if (uiStage == Stage::Analyzing || uiStage == Stage::Thinking)
        return;

    const int bars = ParamChoices::analysisBarsValue ((int) pAnalysisBars->load());
    captureTargetBeats.store (bars * 4);
    captureState.store (capArmed);
    uiStage = Stage::Armed;
    setStatus ("Waiting for the next bar...");
}

void AristoteProcessor::cancelAnalysis()
{
    if (uiStage == Stage::Armed || uiStage == Stage::Listening)
    {
        captureState.store (capIdle);
        uiStage = Stage::Idle;
        setStatus ("Analysis cancelled.");
    }
}

AristoteProcessor::Stage AristoteProcessor::getStage() const { return uiStage; }

float AristoteProcessor::getListenProgress() const
{
    const int t = captureTarget.load();
    if (uiStage != Stage::Listening || t <= 0)
        return uiStage == Stage::Analyzing || uiStage == Stage::Thinking ? 1.0f : 0.0f;
    return juce::jlimit (0.0f, 1.0f, (float) captureWritten.load() / (float) t);
}

AnalysisResult AristoteProcessor::getLastAnalysis() const
{
    const juce::ScopedLock sl (presetLock);
    return lastAnalysis;
}

juce::String AristoteProcessor::getStatusText() const
{
    const juce::ScopedLock sl (presetLock);
    return status;
}

//==============================================================================
std::vector<Preset> AristoteProcessor::getPresets() const
{
    const juce::ScopedLock sl (presetLock);
    return presets;
}

int AristoteProcessor::getActiveIndex() const
{
    const juce::ScopedLock sl (presetLock);
    return activeIndex;
}

Preset AristoteProcessor::getActivePreset() const
{
    const juce::ScopedLock sl (presetLock);
    if (presets.empty())
        return Preset::makeFlat();
    return presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, activeIndex)];
}

void AristoteProcessor::pushActiveTables()
{
    auto tables = std::make_unique<ModulationEngine::Tables>();
    ModulationEngine::render (getActivePreset(), *tables);
    modEngine.setTables (*tables);
}

void AristoteProcessor::selectPreset (int index)
{
    {
        const juce::ScopedLock sl (presetLock);
        if (index < 0 || index >= (int) presets.size())
            return;
        activeIndex = index;
    }
    pushActiveTables();
    sendChangeMessage();
}

void AristoteProcessor::renamePreset (int index, const juce::String& newName)
{
    const auto name = newName.trim().substring (0, 48);
    if (name.isEmpty())
        return;
    {
        const juce::ScopedLock sl (presetLock);
        if (index < 0 || index >= (int) presets.size())
            return;
        presets[(size_t) index].name = name;
    }
    sendChangeMessage();
}

void AristoteProcessor::deletePreset (int index)
{
    {
        const juce::ScopedLock sl (presetLock);
        if (index < 0 || index >= (int) presets.size())
            return;
        presets.erase (presets.begin() + index);
        if (presets.empty())
            presets.push_back (TrapGenerator::makeDefault());
        if (activeIndex >= index && activeIndex > 0)
            --activeIndex;
        activeIndex = juce::jlimit (0, (int) presets.size() - 1, activeIndex);
    }
    pushActiveTables();
    sendChangeMessage();
}

void AristoteProcessor::regeneratePreset (int index)
{
    {
        const juce::ScopedLock sl (presetLock);
        if (index < 0 || index >= (int) presets.size())
            return;

        std::set<juce::String> used;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (i != index)
                used.insert (presets[(size_t) i].name);

        const auto arch = TrapGenerator::archetypeFromName (presets[(size_t) index].archetype);
        const auto seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
        presets[(size_t) index] = TrapGenerator::generateOne (lastAnalysis, arch, seed, used);
    }
    if (index == getActiveIndex())
        pushActiveTables();
    sendChangeMessage();
}

bool AristoteProcessor::savePreset (int index)
{
    Preset p;
    {
        const juce::ScopedLock sl (presetLock);
        if (index < 0 || index >= (int) presets.size())
            return false;
        presets[(size_t) index].saved = true;
        p = presets[(size_t) index];
    }
    sendChangeMessage();

    auto folder = AppSettings::getPresetFolder();
    if (! folder.createDirectory())
        return false;
    auto file = folder.getChildFile (juce::File::createLegalFileName (p.name) + ".atp");
    if (auto xml = p.toValueTree().createXml())
        return xml->writeTo (file);
    return false;
}

int AristoteProcessor::importPresetFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return -1;
    auto p = Preset::fromValueTree (juce::ValueTree::fromXml (*xml));
    p.id = juce::Uuid().toString();
    p.saved = true;
    int idx;
    {
        const juce::ScopedLock sl (presetLock);
        presets.push_back (p);
        idx = (int) presets.size() - 1;
    }
    selectPreset (idx);
    return idx;
}

void AristoteProcessor::updateActivePreset (const Preset& edited)
{
    {
        const juce::ScopedLock sl (presetLock);
        if (presets.empty())
            return;
        auto& p = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, activeIndex)];
        p.lanes = edited.lanes;
        p.lengthBeats = edited.lengthBeats;
    }
    pushActiveTables();
}

//==============================================================================
void AristoteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("AristoteThinker");
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree ps ("Presets");
    {
        const juce::ScopedLock sl (presetLock);
        ps.setProperty ("active", activeIndex, nullptr);
        for (const auto& p : presets)
            ps.appendChild (p.toValueTree(), nullptr);
    }
    root.appendChild (ps, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AristoteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto root = juce::ValueTree::fromXml (*xml);
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params);

    auto ps = root.getChildWithName ("Presets");
    if (ps.isValid() && ps.getNumChildren() > 0)
    {
        const juce::ScopedLock sl (presetLock);
        presets.clear();
        for (const auto& child : ps)
            presets.push_back (Preset::fromValueTree (child));
        activeIndex = juce::jlimit (0, (int) presets.size() - 1, (int) ps.getProperty ("active", 0));
    }
    pushActiveTables();
    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessorEditor* AristoteProcessor::createEditor()
{
    return new AristoteEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AristoteProcessor();
}
