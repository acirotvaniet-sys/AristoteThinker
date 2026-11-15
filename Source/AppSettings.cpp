#include "AppSettings.h"

AppSettings::AppSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "AristoteThinker";
    o.filenameSuffix      = ".settings";
    o.folderName          = "AristoteThinker";
    o.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (o);
}

juce::String AppSettings::getApiKey() const
{
    if (auto* f = const_cast<juce::ApplicationProperties&> (props).getUserSettings())
        return f->getValue ("claudeApiKey");
    return {};
}

void AppSettings::setApiKey (const juce::String& key)
{
    if (auto* f = props.getUserSettings())
    {
        f->setValue ("claudeApiKey", key.trim());
        f->saveIfNeeded();
    }
}

bool AppSettings::isClaudeEnabled() const
{
    if (auto* f = const_cast<juce::ApplicationProperties&> (props).getUserSettings())
        return f->getBoolValue ("useClaude", false);
    return false;
}

void AppSettings::setClaudeEnabled (bool enabled)
{
    if (auto* f = props.getUserSettings())
    {
        f->setValue ("useClaude", enabled);
        f->saveIfNeeded();
    }
}

juce::File AppSettings::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("AristoteThinker")
               .getChildFile ("Presets");
}
