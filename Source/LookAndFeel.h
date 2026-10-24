#pragma once

#include "Theme.h"

class LumenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LumenLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (13.0f); }
    juce::Font getPopupMenuFont() override { return theme::font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};
