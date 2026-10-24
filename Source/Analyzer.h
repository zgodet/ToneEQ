#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// Lock-free mono FIFO the audio thread writes into and the UI drains.
class AnalyzerFifo
{
public:
    void push (const float* left, const float* right, int n)
    {
        int s1, z1, s2, z2;
        fifo.prepareToWrite (n, s1, z1, s2, z2);
        auto write = [&] (int dst, int src, int count)
        {
            for (int i = 0; i < count; ++i)
                buffer[(size_t) (dst + i)] = right != nullptr ? 0.5f * (left[src + i] + right[src + i])
                                                              : left[src + i];
        };
        write (s1, 0, z1);
        write (s2, z1, z2);
        fifo.finishedWrite (z1 + z2);
    }

    int pull (float* dest, int maxCount)
    {
        int s1, z1, s2, z2;
        fifo.prepareToRead (maxCount, s1, z1, s2, z2);
        std::copy_n (buffer.data() + s1, z1, dest);
        std::copy_n (buffer.data() + s2, z2, dest + z1);
        fifo.finishedRead (z1 + z2);
        return z1 + z2;
    }

private:
    static constexpr int size = 1 << 15;
    juce::AbstractFifo fifo { size };
    std::vector<float> buffer = std::vector<float> ((size_t) size, 0.0f);
};

// UI-side FFT analyzer with Pro-Q style 4.5 dB/oct display tilt and ballistics.
class Spectrum
{
public:
    static constexpr int order = 13;
    static constexpr int size = 1 << order;

    Spectrum()
    {
        smoothedDb.assign ((size_t) size / 2 + 1, -150.0f);
    }

    void update (AnalyzerFifo& fifo)
    {
        bool gotData = false;
        float tmp[2048];
        for (;;)
        {
            const int got = fifo.pull (tmp, 2048);
            if (got <= 0) break;
            for (int i = 0; i < got; ++i)
            {
                ring[(size_t) writePos] = tmp[i];
                writePos = (writePos + 1) % size;
            }
            gotData = true;
        }

        if (! gotData)
        {
            for (auto& v : smoothedDb)
                v = std::max (-150.0f, v - 1.5f);
            return;
        }

        for (int i = 0; i < size; ++i)
            fftData[(size_t) i] = ring[(size_t) ((writePos + i) % size)];
        std::fill (fftData.begin() + size, fftData.end(), 0.0f);

        window.multiplyWithWindowingTable (fftData.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const float scale = 4.0f / (float) size;
        for (size_t k = 0; k < smoothedDb.size(); ++k)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[k] * scale, -150.0f);
            float& s = smoothedDb[k];
            s += (db - s) * (db > s ? 0.7f : 0.12f);
        }
    }

    // Level (with display tilt) for the band of frequencies [f0, f1].
    float levelForRange (double f0, double f1, double sampleRate) const
    {
        const double binsPerHz = size / sampleRate;
        const double k0 = std::max (1.0, f0 * binsPerHz);
        const double k1 = std::min ((double) (size / 2), f1 * binsPerHz);
        float level;

        if (k1 - k0 < 1.0)
        {
            const double kc = juce::jlimit (1.0, (double) (size / 2 - 1), 0.5 * (k0 + k1));
            const int i = (int) kc;
            const float frac = (float) (kc - i);
            level = smoothedDb[(size_t) i] + frac * (smoothedDb[(size_t) i + 1] - smoothedDb[(size_t) i]);
        }
        else
        {
            level = -150.0f;
            for (int i = (int) std::ceil (k0); i <= (int) k1; ++i)
                level = std::max (level, smoothedDb[(size_t) i]);
        }

        const double fc = std::sqrt (f0 * f1);
        return level + 4.5f * (float) std::log2 (fc / 1000.0);
    }

private:
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> ring = std::vector<float> ((size_t) size, 0.0f);
    std::vector<float> fftData = std::vector<float> ((size_t) size * 2, 0.0f);
    std::vector<float> smoothedDb;
    int writePos = 0;
};
