#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Analyzer.h"
#include "EQMath.h"

class LumenEQProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 16;

    static juce::String pid (int band, const juce::String& name)
    {
        return "b" + juce::String (band + 1) + "_" + name;
    }

    LumenEQProcessor();
    ~LumenEQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    AnalyzerFifo preFifo, postFifo;
    std::atomic<double> sampleRateForUi { 44100.0 };
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Band
    {
        std::atomic<float>* used = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freqParam = nullptr;
        std::atomic<float>* gainParam = nullptr;
        std::atomic<float>* qParam = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* placement = nullptr;

        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> freq, q;
        juce::SmoothedValue<double> gain;
        juce::SmoothedValue<float> mix;

        int curType = -1, curSlope = -1, curPlacement = 0;
        eq::Design design;
        double z[2][eq::maxSections][2] {};
        bool dirty = true, active = false, silent = true;
        float m0 = 0.0f, m1 = 0.0f;

        void reset (double sampleRate);
        void clearState() { std::memset (z, 0, sizeof (z)); }
        void updateTargets();
        void prepareSegment (int len, double sampleRate);
        void run (float* data, int channel, int len);
    };

    std::array<Band, numBands> bands;
    std::atomic<float>* outputGainParam = nullptr;
    juce::SmoothedValue<float> outputGain;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenEQProcessor)
};
