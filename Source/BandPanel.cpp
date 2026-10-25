#include "BandPanel.h"

void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = ! getToggleState(); // toggle state == bypass
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    const float s = juce::jmin (b.getWidth(), b.getHeight());
    const float cx = b.getCentreX(), cy = b.getCentreY();

    auto c = on ? colour : juce::Colour (0xff5a5f6a);
    if (over) c = c.brighter (0.25f);

    juce::Path arc;
    arc.addCentredArc (cx, cy, s * 0.4f, s * 0.4f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    g.setColour (c);
    g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (cx, cy - s * 0.5f, cx, cy - s * 0.08f, 1.8f);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (over || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.12f : 0.06f));
        g.fillRoundedRectangle (b, 4.0f);
    }

    const float cx = b.getCentreX(), cy = b.getCentreY(), s = 4.5f;
    juce::Path p;
    switch (icon)
    {
        case Icon::Prev:  p.startNewSubPath (cx + s * 0.5f, cy - s); p.lineTo (cx - s * 0.5f, cy); p.lineTo (cx + s * 0.5f, cy + s); break;
        case Icon::Next:  p.startNewSubPath (cx - s * 0.5f, cy - s); p.lineTo (cx + s * 0.5f, cy); p.lineTo (cx - s * 0.5f, cy + s); break;
        case Icon::Close: p.startNewSubPath (cx - s, cy - s); p.lineTo (cx + s, cy + s);
                          p.startNewSubPath (cx + s, cy - s); p.lineTo (cx - s, cy + s); break;
    }
    g.setColour (over ? juce::Colours::white : theme::textDim.brighter (0.3f));
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
BandPanel::BandPanel (ToneEQProcessor& p) : proc (p)
{
    addAndMakeVisible (power);
    power.setTooltip ("Enable / bypass band");

    for (auto* c : { &type, &slope, &placement })
        addAndMakeVisible (*c);
    for (int i = 0; i < eq::numFilterTypes; ++i) type.addItem (eq::filterTypeNames[i], i + 1);
    for (int i = 0; i < eq::numSlopes; ++i) slope.addItem (eq::slopeNames[i], i + 1);
    for (int i = 0; i < eq::numPlacements; ++i) placement.addItem (eq::placementNames[i], i + 1);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        s.setMouseDragSensitivity (220);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (theme::font (10.5f, true));
        l.setColour (juce::Label::textColourId, theme::textDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupKnob (freq, freqLabel, "FREQ");
    setupKnob (gain, gainLabel, "GAIN");
    setupKnob (q, qLabel, "Q");
    gain.getProperties().set ("bipolar", true);

    for (auto* b : { (juce::Button*) &prev, (juce::Button*) &next, (juce::Button*) &close })
        addAndMakeVisible (*b);
    prev.setTooltip ("Previous band");
    next.setTooltip ("Next band");
    close.setTooltip ("Delete band");
    prev.onClick = [this] { if (onNavigate) onNavigate (-1); };
    next.onClick = [this] { if (onNavigate) onNavigate (1); };
    close.onClick = [this] { if (onDelete) onDelete(); };
}

void BandPanel::setBand (int newBand)
{
    if (newBand == band) return;

    powerAtt.reset(); typeAtt.reset(); slopeAtt.reset(); placementAtt.reset();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset();
    band = newBand;
    if (band < 0) return;

    using APVTS = juce::AudioProcessorValueTreeState;
    auto id = [this] (const char* n) { return ToneEQProcessor::pid (band, n); };
    powerAtt     = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id ("bypass"), power);
    typeAtt      = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id ("type"), type);
    slopeAtt     = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id ("slope"), slope);
    placementAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id ("placement"), placement);
    freqAtt      = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id ("freq"), freq);
    gainAtt      = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id ("gain"), gain);
    qAtt         = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id ("q"), q);

    freq.setDoubleClickReturnValue (true, 1000.0);
    gain.setDoubleClickReturnValue (true, 0.0);
    q.setDoubleClickReturnValue (true, 1.0);

    const auto c = theme::band (band);
    power.colour = c;
    for (auto* s : { &freq, &gain, &q })
        s->setColour (juce::Slider::rotarySliderFillColourId, c);

    refresh();
    repaint();
}

void BandPanel::refresh()
{
    if (band < 0) return;
    const auto t = (eq::FilterType) (int) proc.apvts.getRawParameterValue (ToneEQProcessor::pid (band, "type"))->load();
    const bool hasGain = eq::usesGain (t), hasSlope = eq::usesSlope (t);
    if (gain.isEnabled() != hasGain) gain.setEnabled (hasGain);
    if (slope.isEnabled() != hasSlope) slope.setEnabled (hasSlope);
    gainLabel.setAlpha (hasGain ? 1.0f : 0.4f);
    slope.setAlpha (hasSlope ? 1.0f : 0.4f);
}

void BandPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 9.0f);

    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    if (band >= 0)
    {
        const auto c = theme::band (band);
        g.setColour (c);
        g.fillRoundedRectangle (r.getX() + 8.0f, r.getY() + 12.0f, 3.0f, r.getHeight() - 24.0f, 1.5f);

        g.setColour (c);
        g.setFont (theme::font (11.0f, true));
        g.drawText ("BAND " + juce::String (band + 1), juce::Rectangle<float> (r.getRight() - 96.0f, r.getY() + 6.0f, 54.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    // separators
    g.setColour (theme::border);
    g.drawVerticalLine (172, 14.0f, (float) getHeight() - 14.0f);
    g.drawVerticalLine (getWidth() - 48, 14.0f, (float) getHeight() - 14.0f);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 12);
    r.removeFromLeft (10);

    auto left = r.removeFromLeft (148);
    const int rowH = 26, gap = 4;
    auto row1 = left.removeFromTop (rowH);
    power.setBounds (row1.removeFromLeft (rowH));
    row1.removeFromLeft (gap);
    type.setBounds (row1);
    left.removeFromTop (gap * 2);
    slope.setBounds (left.removeFromTop (rowH));
    left.removeFromTop (gap * 2);
    placement.setBounds (left.removeFromTop (rowH));

    auto right = r.removeFromRight (34);
    right.removeFromTop (14);
    prev.setBounds (right.removeFromTop (28));
    next.setBounds (right.removeFromTop (28));
    close.setBounds (right.removeFromTop (28));

    r.removeFromLeft (14);
    r.removeFromRight (14);
    const int colW = r.getWidth() / 3;
    juce::Slider* knobs[] = { &freq, &gain, &q };
    juce::Label* labels[] = { &freqLabel, &gainLabel, &qLabel };
    for (int i = 0; i < 3; ++i)
    {
        auto col = r.removeFromLeft (colW);
        labels[i]->setBounds (col.removeFromTop (14));
        knobs[i]->setBounds (col);
    }
}
