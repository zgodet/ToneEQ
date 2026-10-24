#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

// The main Pro-Q style graph: spectrum analyzer, band + composite curves and
// draggable band nodes.
class EQDisplay : public juce::Component
{
public:
    explicit EQDisplay (LumenEQProcessor&);

    void tick(); // call from a ~60 Hz timer
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    int getSelectedBand() const { return selected; }
    void setSelectedBand (int band);
    void deleteBand (int band);

    void setRange (float db) { range = db; curvesDirty = true; }
    void setAnalyzerMode (int mode) { analyzerMode = mode; } // 0 pre+post, 1 post, 2 pre, 3 off

    std::function<void (int)> onSelectionChanged;

    static constexpr double minFreq = 10.0, maxFreq = 30000.0;

private:
    static constexpr int N = LumenEQProcessor::numBands;

    struct BandView
    {
        bool used = false, bypass = false;
        eq::FilterType type = eq::FilterType::Bell;
        double freq = 1000.0, gain = 0.0, q = 1.0;
        int slope = 1, placement = 0;

        bool operator== (const BandView& o) const
        {
            return used == o.used && bypass == o.bypass && type == o.type && juce::exactlyEqual (freq, o.freq)
                && juce::exactlyEqual (gain, o.gain) && juce::exactlyEqual (q, o.q) && slope == o.slope && placement == o.placement;
        }
        bool operator!= (const BandView& o) const { return ! (*this == o); }
    };

    BandView readBand (int b) const;
    void updateBands (bool force);
    void setParam (int band, const juce::String& name, float value);
    void setParamNoGesture (int band, const juce::String& name, float value);
    int nodeAt (juce::Point<float>) const;
    juce::Point<float> nodePos (int b) const;
    int createBand (double freq, double gain);
    void showBandMenu (int b);
    void beginDrag (int b, juce::Point<float> mouse);
    float compositeDbAt (float x) const;
    bool ghostVisible() const;

    float freqToX (double f) const;
    double xToFreq (float x) const;
    float dbToY (double db) const;
    double yToDb (float y) const;
    float specToY (float db) const;

    LumenEQProcessor& proc;

    float range = 12.0f;
    int analyzerMode = 0;
    bool curvesDirty = true;
    double lastSampleRate = 0.0;

    std::array<BandView, N> views;
    std::array<std::vector<float>, N> bandCurves;
    std::array<float, N> nodeDb {};
    std::vector<float> xs, composite, prePix, postPix;

    Spectrum preSpectrum, postSpectrum;

    int selected = -1, hovered = -1, dragBand = -1;
    bool dragAdjustsQ = false;
    juce::Point<float> dragVirtual, dragLast;
    float dragStartY = 0.0f;
    double dragStartQ = 1.0;
    juce::Point<float> mousePos;
    bool mouseInside = false;
    int createdOnClick = -1;
};
