#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Pure filter math, shared by the audio engine and the UI curve display so
// that what you see is exactly what you hear.
namespace eq
{
enum class FilterType { Bell = 0, LowShelf, LowCut, HighShelf, HighCut, Notch, BandPass, TiltShelf };
constexpr int numFilterTypes = 8;
inline const char* const filterTypeNames[numFilterTypes] = {
    "Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut", "Notch", "Band Pass", "Tilt Shelf"
};

constexpr int numSlopes = 8;
inline const char* const slopeNames[numSlopes] = {
    "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct"
};
constexpr int slopeOrders[numSlopes] = { 1, 2, 3, 4, 6, 8, 12, 16 };

enum class Placement { Stereo = 0, Left, Right, Mid, Side };
constexpr int numPlacements = 5;
inline const char* const placementNames[numPlacements] = { "Stereo", "Left", "Right", "Mid", "Side" };

constexpr int maxSections = 8; // 96 dB/oct = 16th order = 8 biquads

inline bool usesGain (FilterType t)
{
    return t == FilterType::Bell || t == FilterType::LowShelf
        || t == FilterType::HighShelf || t == FilterType::TiltShelf;
}

inline bool usesSlope (FilterType t)
{
    return t == FilterType::LowCut || t == FilterType::HighCut;
}

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    std::complex<double> response (double w) const
    {
        const auto z1 = std::polar (1.0, -w);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

struct Design
{
    std::array<Biquad, maxSections> sections {};
    int numSections = 0;

    double magnitudeDb (double freq, double sampleRate) const
    {
        const double w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        double mag = 1.0;
        for (int i = 0; i < numSections; ++i)
            mag *= std::abs (sections[(size_t) i].response (w));
        return 20.0 * std::log10 (std::max (mag, 1.0e-12));
    }
};

struct BandSettings
{
    FilterType type = FilterType::Bell;
    double freq = 1000.0, gainDb = 0.0, q = 1.0;
    int slope = 1;
};

inline Biquad normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    Biquad b;
    b.b0 = b0 / a0; b.b1 = b1 / a0; b.b2 = b2 / a0;
    b.a1 = a1 / a0; b.a2 = a2 / a0;
    return b;
}

// RBJ cookbook biquads + Butterworth cascades for the cut filters.
inline Design makeDesign (const BandSettings& s, double sampleRate)
{
    constexpr double pi = 3.14159265358979323846;
    Design d;

    const double f = std::clamp (s.freq, 1.0, sampleRate * 0.4999);
    const double w0 = 2.0 * pi * f / sampleRate;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double q = std::max (s.q, 0.01);
    const double alpha = sw / (2.0 * q);

    auto add = [&d] (const Biquad& b)
    {
        if (d.numSections < maxSections)
            d.sections[(size_t) d.numSections++] = b;
    };

    auto lowShelf = [&] (double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sa = 2.0 * std::sqrt (A) * alpha;
        add (normalise (A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sa),
                        (A + 1) + (A - 1) * cw + sa, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa));
    };

    auto highShelf = [&] (double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sa = 2.0 * std::sqrt (A) * alpha;
        add (normalise (A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sa),
                        (A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa));
    };

    switch (s.type)
    {
        case FilterType::Bell:
        {
            const double A = std::pow (10.0, s.gainDb / 40.0);
            add (normalise (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A));
            break;
        }
        case FilterType::LowShelf:  lowShelf (s.gainDb); break;
        case FilterType::HighShelf: highShelf (s.gainDb); break;
        case FilterType::TiltShelf: lowShelf (-s.gainDb * 0.5); highShelf (s.gainDb * 0.5); break;
        case FilterType::Notch:
            add (normalise (1, -2 * cw, 1, 1 + alpha, -2 * cw, 1 - alpha));
            break;
        case FilterType::BandPass:
            add (normalise (alpha, 0, -alpha, 1 + alpha, -2 * cw, 1 - alpha));
            break;
        case FilterType::LowCut:
        case FilterType::HighCut:
        {
            const bool highPass = s.type == FilterType::LowCut;
            const int order = slopeOrders[std::clamp (s.slope, 0, numSlopes - 1)];
            const bool odd = (order % 2) != 0;
            const double resonance = q / std::sqrt (0.5);

            if (odd)
            {
                const double K = std::tan (w0 * 0.5);
                if (highPass) add (normalise (1, -1, 0, 1 + K, K - 1, 0));
                else          add (normalise (K, K, 0, 1 + K, K - 1, 0));
            }

            const int pairs = order / 2;
            for (int k = 1; k <= pairs; ++k)
            {
                const double theta = odd ? pi * k / order : pi * (2 * k - 1) / (2.0 * order);
                double sq = 1.0 / (2.0 * std::cos (theta));
                if (k == pairs) sq *= resonance; // Q acts as resonance on the sharpest pole pair
                const double a = sw / (2.0 * sq);

                if (highPass) add (normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + a, -2 * cw, 1 - a));
                else          add (normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + a, -2 * cw, 1 - a));
            }
            break;
        }
    }

    return d;
}
} // namespace eq
