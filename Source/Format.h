#pragma once

#include <juce_core/juce_core.h>

namespace textfmt
{
inline juce::String freq (double f)
{
    if (f < 1000.0)
        return juce::String (f, f < 100.0 ? 1 : 0) + " Hz";
    return juce::String (f / 1000.0, f < 10000.0 ? 2 : 1) + " kHz";
}

inline juce::String gain (double g)
{
    return (g > 0.049 ? "+" : "") + juce::String (g, 1) + " dB";
}

inline juce::String q (double v)
{
    return juce::String (v, v < 10.0 ? 2 : 1);
}

inline double parseFreq (const juce::String& s)
{
    const auto t = s.trim().toLowerCase();
    double v = t.getDoubleValue();
    if (t.containsChar ('k'))
        v *= 1000.0;
    return v;
}
} // namespace textfmt
