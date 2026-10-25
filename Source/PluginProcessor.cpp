#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Format.h"

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float start, float end, float v) { return start * std::pow (end / start, v); },
             [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
             [] (float start, float end, float v) { return juce::jlimit (start, end, v); } };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ToneEQProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray types, slopes, placements;
    for (auto* n : eq::filterTypeNames) types.add (n);
    for (auto* n : eq::slopeNames) slopes.add (n);
    for (auto* n : eq::placementNames) placements.add (n);

    for (int b = 0; b < numBands; ++b)
    {
        const auto prefix = "Band " + juce::String (b + 1) + " ";
        const float defaultFreq = 30.0f * std::pow (1.5f, (float) b);

        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("band" + juce::String (b + 1),
                                                                            "Band " + juce::String (b + 1), "|");
        group->addChild (
            std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid (b, "used"), 1 }, prefix + "Used", false),
            std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid (b, "bypass"), 1 }, prefix + "Bypass", false),
            std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid (b, "type"), 1 }, prefix + "Shape", types, 0),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pid (b, "freq"), 1 }, prefix + "Frequency", logRange (10.0f, 30000.0f), defaultFreq,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return textfmt::freq (v); })
                    .withValueFromStringFunction ([] (const juce::String& s) { return (float) textfmt::parseFreq (s); })),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pid (b, "gain"), 1 }, prefix + "Gain", juce::NormalisableRange<float> (-30.0f, 30.0f), 0.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return textfmt::gain (v); })
                    .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pid (b, "q"), 1 }, prefix + "Q", logRange (0.025f, 40.0f), 1.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return textfmt::q (v); })
                    .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })),
            std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid (b, "slope"), 1 }, prefix + "Slope", slopes, 1),
            std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid (b, "placement"), 1 }, prefix + "Stereo Placement", placements, 0));

        layout.add (std::move (group));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return textfmt::gain (v); })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));

    return layout;
}

ToneEQProcessor::ToneEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ToneEQ", createLayout())
{
    for (int b = 0; b < numBands; ++b)
    {
        auto& band = bands[(size_t) b];
        band.used      = apvts.getRawParameterValue (pid (b, "used"));
        band.bypass    = apvts.getRawParameterValue (pid (b, "bypass"));
        band.type      = apvts.getRawParameterValue (pid (b, "type"));
        band.freqParam = apvts.getRawParameterValue (pid (b, "freq"));
        band.gainParam = apvts.getRawParameterValue (pid (b, "gain"));
        band.qParam    = apvts.getRawParameterValue (pid (b, "q"));
        band.slope     = apvts.getRawParameterValue (pid (b, "slope"));
        band.placement = apvts.getRawParameterValue (pid (b, "placement"));
    }
    outputGainParam = apvts.getRawParameterValue ("output");
}

bool ToneEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void ToneEQProcessor::Band::reset (double sampleRate)
{
    freq.reset (sampleRate, 0.04);
    q.reset (sampleRate, 0.04);
    gain.reset (sampleRate, 0.04);
    mix.reset (sampleRate, 0.02);

    freq.setCurrentAndTargetValue (freqParam->load());
    q.setCurrentAndTargetValue (qParam->load());
    gain.setCurrentAndTargetValue (gainParam->load());
    mix.setCurrentAndTargetValue (used->load() > 0.5f && bypass->load() < 0.5f ? 1.0f : 0.0f);

    clearState();
    dirty = true;
}

void ToneEQProcessor::Band::updateTargets()
{
    const int t = (int) type->load(), s = (int) slope->load(), p = (int) placement->load();
    if (t != curType || s != curSlope || p != curPlacement)
    {
        curType = t;
        curSlope = s;
        curPlacement = p;
        clearState();
        dirty = true;
    }

    freq.setTargetValue (freqParam->load());
    q.setTargetValue (qParam->load());
    gain.setTargetValue (gainParam->load());
    mix.setTargetValue (used->load() > 0.5f && bypass->load() < 0.5f ? 1.0f : 0.0f);
}

void ToneEQProcessor::Band::prepareSegment (int len, double sampleRate)
{
    const bool smoothing = freq.isSmoothing() || q.isSmoothing() || gain.isSmoothing();
    freq.skip (len);
    q.skip (len);
    gain.skip (len);
    if (smoothing) dirty = true;

    m0 = mix.getCurrentValue();
    m1 = mix.skip (len);
    active = m0 > 0.0f || m1 > 0.0f;

    if (! active)
    {
        if (! silent) { clearState(); silent = true; }
        return;
    }

    silent = false;
    if (dirty)
    {
        eq::BandSettings bs;
        bs.type = (eq::FilterType) curType;
        bs.freq = freq.getCurrentValue();
        bs.gainDb = gain.getCurrentValue();
        bs.q = q.getCurrentValue();
        bs.slope = curSlope;
        design = eq::makeDesign (bs, sampleRate);
        dirty = false;
    }
}

void ToneEQProcessor::Band::run (float* data, int channel, int len)
{
    auto& st = z[channel];
    const int ns = design.numSections;
    const float dm = (m1 - m0) / (float) len;
    float m = m0;

    for (int i = 0; i < len; ++i)
    {
        const double x = data[i];
        double y = x;
        for (int s = 0; s < ns; ++s)
        {
            const auto& c = design.sections[(size_t) s];
            const double out = c.b0 * y + st[s][0];
            st[s][0] = c.b1 * y - c.a1 * out + st[s][1];
            st[s][1] = c.b2 * y - c.a2 * out;
            y = out;
        }
        m += dm;
        data[i] = (float) (x + m * (y - x));
    }
}

//==============================================================================
void ToneEQProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    sampleRateForUi = sampleRate;
    for (auto& b : bands)
    {
        b.curType = -1;
        b.updateTargets();
        b.reset (sampleRate);
    }
    outputGain.reset (sampleRate, 0.03);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainParam->load()));
}

void ToneEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int c = numIn; c < numOut; ++c)
        buffer.clear (c, 0, n);

    const int numCh = juce::jmin (numIn, 2);
    if (numCh == 0 || n == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    preFifo.push (L, R, n);

    for (auto& b : bands)
        b.updateTargets();

    constexpr int subBlock = 32;
    for (int start = 0; start < n; start += subBlock)
    {
        const int len = juce::jmin (subBlock, n - start);
        float* l = L + start;
        float* r = R != nullptr ? R + start : nullptr;
        bool anyMidSide = false;

        // Pass 1: stereo / left / right bands in the L/R domain.
        for (auto& b : bands)
        {
            b.prepareSegment (len, currentSampleRate);
            if (! b.active) continue;

            if (r == nullptr) { b.run (l, 0, len); continue; }

            switch ((eq::Placement) b.curPlacement)
            {
                case eq::Placement::Stereo: b.run (l, 0, len); b.run (r, 1, len); break;
                case eq::Placement::Left:   b.run (l, 0, len); break;
                case eq::Placement::Right:  b.run (r, 1, len); break;
                case eq::Placement::Mid:
                case eq::Placement::Side:   anyMidSide = true; break;
            }
        }

        // Pass 2: mid / side bands in the M/S domain.
        if (anyMidSide)
        {
            for (int i = 0; i < len; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
                l[i] = m;
                r[i] = s;
            }
            for (auto& b : bands)
            {
                if (! b.active) continue;
                if (b.curPlacement == (int) eq::Placement::Mid)  b.run (l, 0, len);
                if (b.curPlacement == (int) eq::Placement::Side) b.run (r, 1, len);
            }
            for (int i = 0; i < len; ++i)
            {
                const float m = l[i], s = r[i];
                l[i] = m + s;
                r[i] = m - s;
            }
        }
    }

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainParam->load()));
    if (outputGain.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = outputGain.getNextValue();
            L[i] *= g;
            if (R != nullptr) R[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (0, 0, n, outputGain.getTargetValue());
        if (R != nullptr) buffer.applyGain (1, 0, n, outputGain.getTargetValue());
    }

    const float pl = buffer.getMagnitude (0, 0, n);
    const float pr = R != nullptr ? buffer.getMagnitude (1, 0, n) : pl;
    peakLeft = juce::jmax (peakLeft.load(), pl);
    peakRight = juce::jmax (peakRight.load(), pr);

    postFifo.push (L, R, n);
}

//==============================================================================
juce::AudioProcessorEditor* ToneEQProcessor::createEditor()
{
    return new ToneEQEditor (*this);
}

void ToneEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ToneEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ToneEQProcessor();
}
