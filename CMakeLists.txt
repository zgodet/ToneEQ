cmake_minimum_required(VERSION 3.22)
project(LumenEQ VERSION 1.0.0)

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

juce_add_plugin(LumenEQ
    COMPANY_NAME "Lumen Audio"
    BUNDLE_ID com.lumenaudio.lumeneq
    PLUGIN_MANUFACTURER_CODE Lmna
    PLUGIN_CODE Lmeq
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Lumen EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(LumenEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EQDisplay.cpp
    Source/BandPanel.cpp
    Source/LookAndFeel.cpp)

target_compile_definitions(LumenEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LumenEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(LUMEN_BUILD_TESTS "Build offline DSP / UI tests" OFF)
if(LUMEN_BUILD_TESTS)
    juce_add_console_app(LumenEQTests PRODUCT_NAME "LumenEQTests")
    target_sources(LumenEQTests PRIVATE
        Tests/PluginTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EQDisplay.cpp
        Source/BandPanel.cpp
        Source/LookAndFeel.cpp)
    target_compile_definitions(LumenEQTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Lumen EQ")
    target_link_libraries(LumenEQTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
