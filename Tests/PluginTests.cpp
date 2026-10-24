// Offline checks: renders sines through the processor and compares measured
// gain with the analytic filter response, then snapshots the editor to PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;

static void setP (LumenEQProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static double measureGainDb (LumenEQProcessor& p, double freq, double sr)
{
    const int block = 512, blocks = 200;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double phase = 0, sumIn = 0, sumOut = 0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = 0.25f * (float) std::sin (phase);
            phase += 2 * juce::MathConstants<double>::pi * freq / sr;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
            if (b >= blocks / 2) sumIn += (double) s * s;
        }
        p.processBlock (buf, midi);
        if (b >= blocks / 2)
            for (int i = 0; i < block; ++i) sumOut += (double) buf.getSample (0, i) * buf.getSample (0, i);
    }
    return 10.0 * std::log10 (sumOut / sumIn);
}

static void check (const char* name, double got, double want, double tol = 0.15)
{
    const bool ok = std::abs (got - want) <= tol;
    if (! ok) ++failures;
    std::printf ("%-44s got %8.2f dB  want %8.2f dB  %s\n", name, got, want, ok ? "ok" : "FAIL");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 48000.0;

    struct Case { const char* name; eq::FilterType t; double f, g, q; int slope; double testF; };
    const Case cases[] = {
        { "Bell +6 dB @1k, at 1k",           eq::FilterType::Bell,     1000, 6, 1, 1, 1000 },
        { "Bell -12 dB @250 Q4, at 250",     eq::FilterType::Bell,      250, -12, 4, 1, 250 },
        { "Low shelf +6 @200, at 30",        eq::FilterType::LowShelf,  200, 6, 0.707, 1, 30 },
        { "High shelf -6 @8k, at 18k",       eq::FilterType::HighShelf, 8000, -6, 0.707, 1, 18000 },
        { "Low cut 24 dB/oct @100, at 100",  eq::FilterType::LowCut,    100, 0, 0.7071, 3, 100 },
        { "Low cut 24 dB/oct @100, at 50",   eq::FilterType::LowCut,    100, 0, 0.7071, 3, 50 },
        { "High cut 48 dB/oct @2k, at 4k",   eq::FilterType::HighCut,  2000, 0, 0.7071, 5, 4000 },
        { "High cut 6 dB/oct @2k, at 2k",    eq::FilterType::HighCut,  2000, 0, 0.7071, 0, 2000 },
        { "Notch @1k, at 900",               eq::FilterType::Notch,    1000, 0, 2, 1, 900 },
        { "Tilt +6 @1k, at 20",              eq::FilterType::TiltShelf, 1000, 6, 0.707, 1, 20 },
    };

    for (const auto& c : cases)
    {
        LumenEQProcessor p;
        setP (p, "b1_used", 1); setP (p, "b1_type", (float) (int) c.t); setP (p, "b1_freq", (float) c.f);
        setP (p, "b1_gain", (float) c.g); setP (p, "b1_q", (float) c.q); setP (p, "b1_slope", (float) c.slope);
        p.prepareToPlay (sr, 512);
        const auto want = eq::makeDesign ({ c.t, c.f, c.g, c.q, c.slope }, sr).magnitudeDb (c.testF, sr);
        check (c.name, measureGainDb (p, c.testF, sr), want);
    }

    // Sanity values for the Butterworth cascades.
    check ("Analytic: LC 24 dB/oct -3 dB at fc", eq::makeDesign ({ eq::FilterType::LowCut, 100, 0, 0.7071, 3 }, sr).magnitudeDb (100, sr), -3.01, 0.05);
    check ("Analytic: LC 24 dB/oct one octave below", eq::makeDesign ({ eq::FilterType::LowCut, 100, 0, 0.7071, 3 }, sr).magnitudeDb (50, sr), -24.1, 0.3);

    {   // Mid/side: a side-only cut must leave a mono signal untouched.
        LumenEQProcessor p;
        setP (p, "b1_used", 1); setP (p, "b1_type", 0); setP (p, "b1_freq", 1000); setP (p, "b1_gain", -12); setP (p, "b1_placement", 4);
        p.prepareToPlay (sr, 512);
        check ("Side-only bell on mono signal", measureGainDb (p, 1000, sr), 0.0, 0.05);
    }
    {   // Bypassed band and output gain.
        LumenEQProcessor p;
        setP (p, "b1_used", 1); setP (p, "b1_gain", 12); setP (p, "b1_freq", 1000); setP (p, "b1_bypass", 1); setP (p, "output", -6);
        p.prepareToPlay (sr, 512);
        check ("Bypassed band + output -6 dB", measureGainDb (p, 1000, sr), -6.0, 0.05);
    }

    // Editor snapshot with a few bands.
    {
        LumenEQProcessor p;
        auto band = [&] (int b, int type, float f, float g, float q, int slope = 1, int pl = 0)
        {
            auto id = [b] (const char* n) { return LumenEQProcessor::pid (b, n); };
            setP (p, id ("used"), 1); setP (p, id ("type"), (float) type); setP (p, id ("freq"), f);
            setP (p, id ("gain"), g); setP (p, id ("q"), q); setP (p, id ("slope"), (float) slope); setP (p, id ("placement"), (float) pl);
        };
        band (0, 2, 32, 0, 0.9f, 3);
        band (1, 1, 110, 3.5f, 0.8f);
        band (2, 0, 420, -4.5f, 2.2f);
        band (3, 0, 2600, 3.0f, 1.2f, 1, 3);
        band (4, 3, 9000, 2.5f, 0.7f);
        band (5, 4, 17000, 0, 0.7071f, 4);
        p.prepareToPlay (sr, 512);

        // Pink-ish noise so the analyzer has something to draw.
        juce::Random rnd (1);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        float b0 = 0, b1 = 0, b2 = 0;

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* editor = dynamic_cast<LumenEQEditor*> (ed.get());
        ed->setSize (1060, 620);
        for (int frame = 0; frame < 40; ++frame)
        {
            for (int k = 0; k < 4; ++k)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float w = rnd.nextFloat() * 2 - 1;
                    b0 = 0.99765f * b0 + w * 0.099046f; b1 = 0.963f * b1 + w * 0.2965164f; b2 = 0.57f * b2 + w * 1.0526913f;
                    const float s = 0.08f * (b0 + b1 + b2 + w * 0.1848f);
                    buf.setSample (0, i, s); buf.setSample (1, i, s);
                }
                p.processBlock (buf, midi);
            }
            juce::ignoreUnused (editor);
            for (auto* c : ed->getChildren())
                if (auto* d = dynamic_cast<EQDisplay*> (c)) { d->tick(); if (frame == 39) d->setSelectedBand (2); }
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (argc > 1 ? argv[1] : "screenshot.png");
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("Wrote %s\n", out.getFullPathName().toRawUTF8());
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
