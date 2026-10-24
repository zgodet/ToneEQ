#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("power") { setClickingTogglesState (true); }
    juce::Colour colour = juce::Colours::white;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class IconButton : public juce::Button
{
public:
    enum class Icon { Prev, Next, Close };
    explicit IconButton (Icon i) : juce::Button ("icon"), icon (i) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
};

// Floating parameter panel for the selected band (bottom-centre, Pro-Q style).
class BandPanel : public juce::Component
{
public:
    explicit BandPanel (LumenEQProcessor&);

    void setBand (int band);
    int getBand() const { return band; }
    void refresh(); // keep enabled/visible state in sync with the band shape

    std::function<void (int direction)> onNavigate;
    std::function<void()> onDelete;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int panelWidth = 600, panelHeight = 118;

private:
    LumenEQProcessor& proc;
    int band = -1;

    PowerButton power;
    juce::ComboBox type, slope, placement;
    juce::Slider freq, gain, q;
    juce::Label freqLabel, gainLabel, qLabel;
    IconButton prev { IconButton::Icon::Prev }, next { IconButton::Icon::Next }, close { IconButton::Icon::Close };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt, placementAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
};
