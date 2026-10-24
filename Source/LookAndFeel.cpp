#include "LookAndFeel.h"

LumenLookAndFeel::LumenLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::ComboBox::backgroundColourId, theme::control);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d2027));
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff30353f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, theme::control);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a2f38));
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::accent);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, theme::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::backgroundColourId, theme::control);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
    setColour (juce::CaretComponent::caretColourId, theme::accent);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff23262e));
    setColour (juce::TooltipWindow::textColourId, theme::text);
}

void LumenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const float size = (float) juce::jmin (w, h);
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float r = size * 0.5f - 3.0f;
    if (r < 6.0f) return;

    auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    if (! slider.isEnabled())
        colour = colour.withSaturation (0.0f).withAlpha (0.35f);

    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = (bool) slider.getProperties()["bipolar"];
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    juce::Path track;
    track.addCentredArc (cx, cy, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2e36));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (cx, cy, r, r, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (colour);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - 6.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b404a), cx, cy - kr, juce::Colour (0xff1c1f25), cx, cy + kr, false));
    g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff0b0c0f));
    g.drawEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.25f, -kr + 3.0f, 2.5f, kr * 0.45f, 1.0f);
    g.setColour (slider.isEnabled() ? colour.brighter (0.3f) : colour);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void LumenLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float,
                                         float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
        return;
    }

    const float cy = (float) y + (float) h * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (juce::Colour (0xff2a2e36));
    g.fillRoundedRectangle (track, 2.0f);

    const float zeroX = (float) x + (float) w * (float) slider.valueToProportionOfLength (0.0);
    g.setColour (theme::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (zeroX, sliderPos), cy - 2.0f,
                                                    std::abs (sliderPos - zeroX), 4.0f), 2.0f);
    g.setColour (theme::text);
    g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
}

void LumenLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? theme::border.brighter (0.3f) : theme::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.3f));
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LumenLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void LumenLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto bg = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : findColour (juce::TextButton::buttonColourId);
    if (down) bg = bg.brighter (0.15f);
    else if (over) bg = bg.brighter (0.07f);
    g.setColour (bg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (over ? theme::border.brighter (0.3f) : theme::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LumenLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (theme::border);
    g.drawRect (0, 0, w, h);
}

juce::Label* LumenLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::font (12.5f));
    l->setColour (juce::Label::textColourId, theme::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineWhenEditingColourId, theme::accent);
    return l;
}
