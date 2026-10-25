#pragma once

#include "BandPanel.h"
#include "EQDisplay.h"
#include "LookAndFeel.h"
#include "PluginProcessor.h"

class LevelMeter : public juce::Component
{
public:
    void setPeaks (float left, float right);
    void paint (juce::Graphics&) override;

private:
    float levels[2] { -60.0f, -60.0f }, holds[2] { -60.0f, -60.0f };
    int holdFrames[2] {};
};

class ToneEQEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ToneEQEditor (ToneEQProcessor&);
    ~ToneEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateAnalyzerButton();
    void updateRangeButton();
    void navigate (int direction);

    ToneEQProcessor& proc;
    ToneLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    EQDisplay display;
    BandPanel panel;
    LevelMeter meter;

    juce::TextButton analyzerButton, rangeButton, resetButton { "Reset" };
    juce::Slider outputSlider;
    juce::Label outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAtt;

    int analyzerMode = 0, rangeIndex = 2;

    static constexpr int topBarHeight = 38, bottomBarHeight = 34;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneEQEditor)
};
