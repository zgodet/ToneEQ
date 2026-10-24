#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
const juce::Colour background   { 0xff111317 };
const juce::Colour backgroundHi { 0xff1a1d23 };
const juce::Colour bar          { 0xff17191e };
const juce::Colour panel        { 0xf01d2027 };
const juce::Colour border       { 0xff2c3039 };
const juce::Colour control      { 0xff15171b };
const juce::Colour text         { 0xffc9cdd6 };
const juce::Colour textDim      { 0xff6f7581 };
const juce::Colour curve        { 0xfff4ead2 };
const juce::Colour accent       { 0xfff2c25a };

inline juce::Colour band (int i)
{
    static const juce::uint32 colours[] = {
        0xffff6b6b, 0xffffa64d, 0xffffd84d, 0xffb4e34d, 0xff5fe08a, 0xff4de0c8, 0xff4dc3ff, 0xff6b8cff,
        0xffa47cff, 0xffe07cff, 0xffff7cb8, 0xffff8a7c, 0xffd8c27c, 0xff8ce0b4, 0xff7cc8e0, 0xffc0a8ff
    };
    return juce::Colour (colours[(size_t) (i % 16)]);
}

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace theme
