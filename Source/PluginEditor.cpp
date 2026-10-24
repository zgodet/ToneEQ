#include "PluginEditor.h"

namespace
{
const float rangeOptions[] = { 3.0f, 6.0f, 12.0f, 18.0f, 30.0f };
const char* const analyzerNames[] = { "Pre + Post", "Post", "Pre", "Off" };
} // namespace

//==============================================================================
void LevelMeter::setPeaks (float left, float right)
{
    const float in[2] { left, right };
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (in[c], -60.0f);
        levels[c] = db > levels[c] ? db : juce::jmax (-60.0f, levels[c] - 0.8f);
        if (db >= holds[c]) { holds[c] = db; holdFrames[c] = 60; }
        else if (--holdFrames[c] <= 0) holds[c] = juce::jmax (-60.0f, holds[c] - 0.5f);
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0d10));
    g.fillRect (b);

    auto toY = [&] (float db) { return juce::jmap (db, 6.0f, -60.0f, b.getY() + 2.0f, b.getBottom() - 2.0f); };
    const float barW = (b.getWidth() - 6.0f) / 2.0f;

    for (int c = 0; c < 2; ++c)
    {
        const float x = b.getX() + 2.0f + (float) c * (barW + 2.0f);
        const float y = toY (juce::jmin (levels[c], 6.0f));
        juce::ColourGradient grad (juce::Colour (0xffff4d4d), 0, toY (6.0f), juce::Colour (0xff3fbf7f), 0, toY (-60.0f), false);
        grad.addColour (juce::jmap (0.0f, 6.0f, -60.0f, 0.0f, 1.0f), juce::Colour (0xffffc24d));
        grad.addColour (juce::jmap (-12.0f, 6.0f, -60.0f, 0.0f, 1.0f), juce::Colour (0xff7fdc6a));
        g.setGradientFill (grad);
        g.fillRect (juce::Rectangle<float> (x, y, barW, b.getBottom() - 2.0f - y));

        g.setColour (holds[c] > 0.0f ? juce::Colour (0xffff4d4d) : theme::text);
        g.fillRect (juce::Rectangle<float> (x, toY (juce::jmin (holds[c], 6.0f)), barW, 1.5f));
    }

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawHorizontalLine ((int) toY (0.0f), b.getX(), b.getRight());
}

//==============================================================================
LumenEQEditor::LumenEQEditor (LumenEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), panel (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);

    addAndMakeVisible (display);
    addChildComponent (panel);
    addAndMakeVisible (meter);

    display.onSelectionChanged = [this] (int band)
    {
        panel.setBand (band);
        panel.setVisible (band >= 0);
    };
    panel.onNavigate = [this] (int dir) { navigate (dir); };
    panel.onDelete = [this] { display.deleteBand (display.getSelectedBand()); };

    analyzerMode = (int) proc.apvts.state.getProperty ("analyzerMode", 0);
    rangeIndex = (int) proc.apvts.state.getProperty ("rangeIndex", 2);
    analyzerMode = juce::jlimit (0, 3, analyzerMode);
    rangeIndex = juce::jlimit (0, 4, rangeIndex);
    display.setAnalyzerMode (analyzerMode);
    display.setRange (rangeOptions[rangeIndex]);

    addAndMakeVisible (analyzerButton);
    analyzerButton.setTooltip ("Analyzer source");
    analyzerButton.onClick = [this]
    {
        analyzerMode = (analyzerMode + 1) % 4;
        proc.apvts.state.setProperty ("analyzerMode", analyzerMode, nullptr);
        display.setAnalyzerMode (analyzerMode);
        updateAnalyzerButton();
    };
    updateAnalyzerButton();

    addAndMakeVisible (rangeButton);
    rangeButton.setTooltip ("Display range");
    rangeButton.onClick = [this]
    {
        rangeIndex = (rangeIndex + 1) % 5;
        proc.apvts.state.setProperty ("rangeIndex", rangeIndex, nullptr);
        display.setRange (rangeOptions[rangeIndex]);
        updateRangeButton();
    };
    updateRangeButton();

    addAndMakeVisible (resetButton);
    resetButton.setTooltip ("Remove all bands");
    resetButton.onClick = [this]
    {
        display.setSelectedBand (-1);
        for (int b = 0; b < LumenEQProcessor::numBands; ++b)
            if (auto* prm = proc.apvts.getParameter (LumenEQProcessor::pid (b, "used")))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (0.0f);
                prm->endChangeGesture();
            }
    };

    outputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (outputSlider);
    outputAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "output", outputSlider);

    outputLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputLabel.setFont (theme::font (11.0f, true));
    outputLabel.setColour (juce::Label::textColourId, theme::textDim);
    outputLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (outputLabel);

    setResizable (true, true);
    setResizeLimits (760, 440, 2400, 1400);
    setSize (1060, 620);

    startTimerHz (60);
}

LumenEQEditor::~LumenEQEditor()
{
    stopTimer();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void LumenEQEditor::updateAnalyzerButton()
{
    analyzerButton.setButtonText (juce::String ("Analyzer: ") + analyzerNames[analyzerMode]);
}

void LumenEQEditor::updateRangeButton()
{
    rangeButton.setButtonText ("Range: " + juce::String ((int) rangeOptions[rangeIndex]) + " dB");
}

void LumenEQEditor::navigate (int direction)
{
    const int n = LumenEQProcessor::numBands;
    const int start = display.getSelectedBand();
    for (int i = 1; i <= n; ++i)
    {
        const int b = ((start + direction * i) % n + n) % n;
        if (proc.apvts.getRawParameterValue (LumenEQProcessor::pid (b, "used"))->load() > 0.5f)
        {
            display.setSelectedBand (b);
            return;
        }
    }
}

void LumenEQEditor::timerCallback()
{
    display.tick();
    panel.refresh();
    meter.setPeaks (proc.peakLeft.exchange (0.0f), proc.peakRight.exchange (0.0f));
}

void LumenEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto top = getLocalBounds().removeFromTop (topBarHeight);
    g.setColour (theme::bar);
    g.fillRect (top);
    g.setColour (theme::border);
    g.drawHorizontalLine (top.getBottom() - 1, 0.0f, (float) getWidth());

    // Logo
    auto logo = top.reduced (14, 0).removeFromLeft (220).toFloat();
    const float cy = logo.getCentreY();
    juce::Path mark;
    mark.startNewSubPath (logo.getX(), cy + 4.0f);
    mark.cubicTo (logo.getX() + 6.0f, cy + 4.0f, logo.getX() + 7.0f, cy - 7.0f, logo.getX() + 11.0f, cy - 7.0f);
    mark.cubicTo (logo.getX() + 15.0f, cy - 7.0f, logo.getX() + 16.0f, cy + 4.0f, logo.getX() + 22.0f, cy + 4.0f);
    g.setColour (theme::accent);
    g.strokePath (mark, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (theme::font (17.0f, true));
    g.setColour (theme::text);
    g.drawText ("LUMEN", logo.withTrimmedLeft (32.0f), juce::Justification::centredLeft);
    g.setColour (theme::accent);
    g.drawText ("EQ", logo.withTrimmedLeft (92.0f), juce::Justification::centredLeft);

    auto bottom = getLocalBounds().removeFromBottom (bottomBarHeight);
    g.setColour (theme::bar);
    g.fillRect (bottom);
    g.setColour (theme::border);
    g.drawHorizontalLine (bottom.getY(), 0.0f, (float) getWidth());

    g.setFont (theme::font (11.5f));
    g.setColour (theme::textDim);
    g.drawText ("Double-click or click the curve to add a band  -  drag to move  -  scroll for Q  -  shift for fine  -  alt-click / delete to remove",
                bottom.reduced (320, 0), juce::Justification::centred);
}

void LumenEQEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (topBarHeight).reduced (10, 6);
    resetButton.setBounds (top.removeFromRight (70));

    auto bottom = r.removeFromBottom (bottomBarHeight).reduced (10, 5);
    analyzerButton.setBounds (bottom.removeFromLeft (150));
    bottom.removeFromLeft (6);
    rangeButton.setBounds (bottom.removeFromLeft (120));
    outputSlider.setBounds (bottom.removeFromRight (190));
    outputLabel.setBounds (bottom.removeFromRight (60));

    meter.setBounds (r.removeFromRight (16));
    display.setBounds (r);

    const int pw = juce::jmin (BandPanel::panelWidth, r.getWidth() - 20);
    panel.setBounds (r.getCentreX() - pw / 2, r.getBottom() - BandPanel::panelHeight - 30, pw, BandPanel::panelHeight);
}
