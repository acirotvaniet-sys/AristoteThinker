cmake_minimum_required(VERSION 3.22)

project(AristoteThinker VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE -------------------------------------------------------------------
# Option 1 : un JUCE local   -> cmake -B build -DJUCE_PATH=C:/dev/JUCE
# Option 2 : par défaut, JUCE est téléchargé automatiquement (FetchContent).
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
set(ARISTOTE_JUCE_TAG "8.0.17-security" CACHE STRING "JUCE git tag used when JUCE_PATH is empty")

if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${ARISTOTE_JUCE_TAG}
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin -----------------------------------------------------------------
if(APPLE)
    set(ARISTOTE_FORMATS VST3 AU Standalone)
else()
    set(ARISTOTE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(AristoteThinker
    COMPANY_NAME                "AristoteAudio"
    PLUGIN_MANUFACTURER_CODE    Arst
    PLUGIN_CODE                 AThk
    FORMATS                     ${ARISTOTE_FORMATS}  # AU ajouté automatiquement sur macOS
    BUNDLE_ID                   com.aristoteaudio.aristotethinker
    AU_MAIN_TYPE                kAudioUnitType_Effect
    PRODUCT_NAME                "AristoteThinker"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Modulation)

target_sources(AristoteThinker PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/AppSettings.cpp
    Source/model/Preset.cpp
    Source/analysis/ChordAnalyzer.cpp
    Source/generation/TrapGenerator.cpp
    Source/generation/ClaudeAdvisor.cpp
    Source/modulation/ModulationEngine.cpp
    Source/fx/FxChain.cpp
    Source/ui/AristoteLookAndFeel.cpp
    Source/ui/AristotleHead.cpp
    Source/ui/CurveEditor.cpp
    Source/ui/GenerationTab.cpp
    Source/ui/FxTab.cpp)

target_include_directories(AristoteThinker PRIVATE Source)

target_compile_definitions(AristoteThinker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AristoteThinker
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests (optionnel) --------------------------------------------------------
option(ARISTOTE_BUILD_TESTS "Build the analysis/generation console test" OFF)
if(ARISTOTE_BUILD_TESTS)
    juce_add_console_app(AristoteTests PRODUCT_NAME "AristoteTests")
    target_sources(AristoteTests PRIVATE
        tests/AnalyzerTest.cpp
        Source/model/Preset.cpp
        Source/analysis/ChordAnalyzer.cpp
        Source/generation/TrapGenerator.cpp)
    target_include_directories(AristoteTests PRIVATE Source)
    target_compile_definitions(AristoteTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(AristoteTests PRIVATE juce::juce_dsp juce::juce_data_structures
                          PUBLIC juce::juce_recommended_config_flags)

    # Rendu PNG des 6 expressions d'Aristote
    juce_add_console_app(AristoteHeadSnapshot PRODUCT_NAME "AristoteHeadSnapshot")
    target_sources(AristoteHeadSnapshot PRIVATE
        tests/HeadSnapshot.cpp
        Source/model/Preset.cpp
        Source/ui/AristotleHead.cpp
        Source/ui/AristoteLookAndFeel.cpp)
    target_include_directories(AristoteHeadSnapshot PRIVATE Source)
    target_compile_definitions(AristoteHeadSnapshot PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(AristoteHeadSnapshot PRIVATE juce::juce_gui_basics
                          PUBLIC juce::juce_recommended_config_flags)
endif()
