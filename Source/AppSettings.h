#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Réglages globaux de l'utilisateur (hors projet DAW) : clé API Claude, etc.
// Stockés dans %APPDATA%\AristoteThinker\AristoteThinker.settings (texte clair).
class AppSettings
{
public:
    AppSettings();

    juce::String getApiKey() const;
    void setApiKey (const juce::String& key);

    bool isClaudeEnabled() const;
    void setClaudeEnabled (bool enabled);

    static juce::File getPresetFolder();

private:
    juce::ApplicationProperties props;
};
