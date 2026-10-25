cmake_minimum_required(VERSION 3.22)
project(ToneEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if present, otherwise download it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ToneEQ
    COMPANY_NAME "Audemo"
    BUNDLE_ID com.audemo.toneeq
    PLUGIN_MANUFACTURER_CODE Audm
    PLUGIN_CODE Tneq
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ToneEQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(ToneEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EQDisplay.cpp
    Source/BandPanel.cpp
    Source/LookAndFeel.cpp)

target_compile_definitions(ToneEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ToneEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(TONEEQ_BUILD_TESTS "Build offline DSP / UI tests" OFF)
if(TONEEQ_BUILD_TESTS)
    juce_add_console_app(ToneEQTests PRODUCT_NAME "ToneEQTests")
    target_sources(ToneEQTests PRIVATE
        Tests/PluginTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EQDisplay.cpp
        Source/BandPanel.cpp
        Source/LookAndFeel.cpp)
    target_compile_definitions(ToneEQTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="ToneEQ")
    target_link_libraries(ToneEQTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
