cmake_minimum_required(VERSION 3.22)

project(Epatoles VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Epatoles
    COMPANY_NAME                "Songcraft Audio"
    BUNDLE_ID                   com.songcraftaudio.epatoles
    PLUGIN_MANUFACTURER_CODE    Sngc
    PLUGIN_CODE                 Epat
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Epatoles"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     TRUE
)

target_sources(Epatoles PRIVATE
    EpatolesProcessor.cpp
    EpatolesProcessor.h
    TapeHeads.h
    PluginEditor.cpp
    PluginEditor.h
)

target_compile_definitions(Epatoles PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Epatoles
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
