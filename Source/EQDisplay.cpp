#include "EQDisplay.h"
#include "Format.h"

EQDisplay::EQDisplay (LumenEQProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setRepaintsOnMouseActivity (false);
}

//==============================================================================
// Coordinate mapping
float EQDisplay::freqToX (double f) const
{
    return (float) (std::log (f / minFreq) / std::log (maxFreq / minFreq)) * (float) getWidth();
}

double EQDisplay::xToFreq (float x) const
{
    return minFreq * std::pow (maxFreq / minFreq, (double) x / (double) juce::jmax (1, getWidth()));
}

float EQDisplay::dbToY (double db) const
{
    const float top = 18.0f, bottom = (float) getHeight() - 30.0f;
    const float mid = (top + bottom) * 0.5f;
    return mid - (float) (db / range) * (bottom - top) * 0.5f;
}

double EQDisplay::yToDb (float y) const
{
    const float top = 18.0f, bottom = (float) getHeight() - 30.0f;
    const float mid = (top + bottom) * 0.5f;
    return (double) ((mid - y) / ((bottom - top) * 0.5f) * range);
}

float EQDisplay::specToY (float db) const
{
    constexpr float topDb = 12.0f, bottomDb = -96.0f;
    return juce::jmap (db, topDb, bottomDb, 0.0f, (float) getHeight());
}

//==============================================================================
EQDisplay::BandView EQDisplay::readBand (int b) const
{
    auto get = [this, b] (const char* n) { return proc.apvts.getRawParameterValue (LumenEQProcessor::pid (b, n))->load(); };
    BandView v;
    v.used = get ("used") > 0.5f;
    v.bypass = get ("bypass") > 0.5f;
    v.type = (eq::FilterType) (int) get ("type");
    v.freq = get ("freq");
    v.gain = get ("gain");
    v.q = get ("q");
    v.slope = (int) get ("slope");
    v.placement = (int) get ("placement");
    return v;
}

void EQDisplay::setParamNoGesture (int band, const juce::String& name, float value)
{
    if (auto* p = proc.apvts.getParameter (LumenEQProcessor::pid (band, name)))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EQDisplay::setParam (int band, const juce::String& name, float value)
{
    if (auto* p = proc.apvts.getParameter (LumenEQProcessor::pid (band, name)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void EQDisplay::resized()
{
    const int numPts = juce::jmax (2, getWidth() / 2 + 2);
    xs.resize ((size_t) numPts);
    for (int i = 0; i < numPts; ++i)
        xs[(size_t) i] = (float) i * 2.0f;
    for (auto& c : bandCurves) c.assign ((size_t) numPts, 0.0f);
    composite.assign ((size_t) numPts, 0.0f);
    prePix.assign ((size_t) numPts, -150.0f);
    postPix.assign ((size_t) numPts, -150.0f);
    curvesDirty = true;
}

void EQDisplay::updateBands (bool force)
{
    const double sr = proc.sampleRateForUi.load();
    if (! juce::exactlyEqual (sr, lastSampleRate)) { lastSampleRate = sr; force = true; }
    if (curvesDirty) { force = true; curvesDirty = false; }

    bool changed = false;
    for (int b = 0; b < N; ++b)
    {
        const auto v = readBand (b);
        if (! force && v == views[(size_t) b]) continue;

        views[(size_t) b] = v;
        changed = true;
        auto& curve = bandCurves[(size_t) b];
        if (! v.used) { std::fill (curve.begin(), curve.end(), 0.0f); continue; }

        eq::BandSettings s { v.type, v.freq, v.gain, v.q, v.slope };
        const auto design = eq::makeDesign (s, sr);
        for (size_t k = 0; k < xs.size(); ++k)
            curve[k] = (float) design.magnitudeDb (xToFreq (xs[k]), sr);

        nodeDb[(size_t) b] = eq::usesGain (v.type) ? (float) v.gain
                                                   : (float) design.magnitudeDb (v.freq, sr);
    }

    if (changed)
    {
        std::fill (composite.begin(), composite.end(), 0.0f);
        for (int b = 0; b < N; ++b)
            if (views[(size_t) b].used && ! views[(size_t) b].bypass)
                for (size_t k = 0; k < xs.size(); ++k)
                    composite[k] += bandCurves[(size_t) b][k];
    }

    if (selected >= 0 && ! views[(size_t) selected].used)
        setSelectedBand (-1);
}

void EQDisplay::tick()
{
    updateBands (false);

    // Always drain the FIFOs so data never goes stale.
    preSpectrum.update (proc.preFifo);
    postSpectrum.update (proc.postFifo);

    if (analyzerMode != 3)
    {
        const double sr = proc.sampleRateForUi.load();
        for (size_t k = 0; k < xs.size(); ++k)
        {
            const double f0 = xToFreq (xs[k] - 1.0f), f1 = xToFreq (xs[k] + 1.0f);
            prePix[k] = preSpectrum.levelForRange (f0, f1, sr);
            postPix[k] = postSpectrum.levelForRange (f0, f1, sr);
        }
    }

    repaint();
}

//==============================================================================
juce::Point<float> EQDisplay::nodePos (int b) const
{
    const float y = juce::jlimit (6.0f, (float) getHeight() - 6.0f, dbToY (nodeDb[(size_t) b]));
    return { freqToX (views[(size_t) b].freq), y };
}

int EQDisplay::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 11.0f;
    for (int b = 0; b < N; ++b)
    {
        if (! views[(size_t) b].used) continue;
        const float d = nodePos (b).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = b; }
    }
    return best;
}

float EQDisplay::compositeDbAt (float x) const
{
    if (composite.empty()) return 0.0f;
    const float idx = juce::jlimit (0.0f, (float) composite.size() - 1.001f, x * 0.5f);
    const int i = (int) idx;
    const float frac = idx - (float) i;
    return composite[(size_t) i] + frac * (composite[(size_t) i + 1] - composite[(size_t) i]);
}

bool EQDisplay::ghostVisible() const
{
    if (! mouseInside || hovered >= 0 || dragBand >= 0) return false;
    return std::abs (mousePos.y - dbToY (compositeDbAt (mousePos.x))) < 14.0f;
}

void EQDisplay::setSelectedBand (int band)
{
    if (band == selected) return;
    selected = band;
    if (onSelectionChanged) onSelectionChanged (band);
    repaint();
}

void EQDisplay::deleteBand (int band)
{
    if (band < 0) return;
    setParam (band, "used", 0.0f);
    if (selected == band) setSelectedBand (-1);
    if (hovered == band) hovered = -1;
    updateBands (true);
}

int EQDisplay::createBand (double freq, double gain)
{
    int band = -1;
    for (int b = 0; b < N; ++b)
        if (proc.apvts.getRawParameterValue (LumenEQProcessor::pid (b, "used"))->load() < 0.5f) { band = b; break; }
    if (band < 0) return -1;

    auto type = eq::FilterType::Bell;
    if (freq < 30.0) type = eq::FilterType::LowCut;
    else if (freq > 15000.0) type = eq::FilterType::HighCut;

    freq = juce::jlimit (minFreq, maxFreq, freq);
    setParam (band, "type", (float) (int) type);
    setParam (band, "freq", (float) freq);
    setParam (band, "gain", eq::usesGain (type) ? (float) juce::jlimit (-30.0, 30.0, gain) : 0.0f);
    setParam (band, "q", eq::usesSlope (type) ? 0.7071f : 1.0f);
    setParam (band, "slope", 1.0f);
    setParam (band, "placement", 0.0f);
    setParam (band, "bypass", 0.0f);
    setParam (band, "used", 1.0f);

    updateBands (true);
    setSelectedBand (band);
    return band;
}

void EQDisplay::beginDrag (int b, juce::Point<float> mouse)
{
    dragBand = b;
    dragAdjustsQ = ! eq::usesGain (views[(size_t) b].type);
    dragVirtual = nodePos (b);
    dragLast = mouse;
    dragStartY = dragVirtual.y;
    dragStartQ = views[(size_t) b].q;

    for (auto* n : { "freq", "gain", "q" })
        if (auto* p = proc.apvts.getParameter (LumenEQProcessor::pid (b, n)))
            p->beginChangeGesture();
}

//==============================================================================
// Mouse handling
void EQDisplay::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    mouseInside = true;
    hovered = nodeAt (e.position);
    setMouseCursor (hovered >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EQDisplay::mouseExit (const juce::MouseEvent&)
{
    mouseInside = false;
    hovered = -1;
}

void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    mousePos = e.position;
    if (e.getNumberOfClicks() == 1) createdOnClick = -1;
    const int b = nodeAt (e.position);

    if (b >= 0)
    {
        setSelectedBand (b);
        if (e.mods.isPopupMenu()) { showBandMenu (b); return; }
        if (e.mods.isAltDown()) { deleteBand (b); return; }
        beginDrag (b, e.position);
        return;
    }

    if (e.mods.isPopupMenu()) return;

    // Click on the ghost node on the curve: create a band and start dragging it.
    if (ghostVisible() && e.getNumberOfClicks() == 1)
    {
        const int nb = createBand (xToFreq (e.position.x), 0.0);
        if (nb >= 0) beginDrag (nb, e.position);
        createdOnClick = nb;
        return;
    }

    setSelectedBand (-1);
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (dragBand < 0) return;

    const float scale = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const auto delta = (e.position - dragLast) * scale;
    dragLast = e.position;
    dragVirtual += delta;

    const auto& v = views[(size_t) dragBand];
    setParamNoGesture (dragBand, "freq", (float) juce::jlimit (minFreq, maxFreq, xToFreq (dragVirtual.x)));

    if (dragAdjustsQ)
    {
        const double q = dragStartQ * std::pow (2.0, (dragStartY - dragVirtual.y) / 40.0);
        setParamNoGesture (dragBand, "q", (float) juce::jlimit (0.025, 40.0, q));
    }
    else
    {
        setParamNoGesture (dragBand, "gain", (float) juce::jlimit (-30.0, 30.0, yToDb (dragVirtual.y)));
    }
    juce::ignoreUnused (v);
    updateBands (false);
}

void EQDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (dragBand >= 0)
    {
        for (auto* n : { "freq", "gain", "q" })
            if (auto* p = proc.apvts.getParameter (LumenEQProcessor::pid (dragBand, n)))
                p->endChangeGesture();
        dragBand = -1;
    }
    hovered = nodeAt (e.position);
}

void EQDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = nodeAt (e.position);
    if (b >= 0)
    {
        if (b != createdOnClick)
            setParam (b, "bypass", views[(size_t) b].bypass ? 0.0f : 1.0f);
        return;
    }
    if (dragBand < 0)
        createBand (xToFreq (e.position.x), yToDb (e.position.y));
}

void EQDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int target = hovered >= 0 ? hovered : nodeAt (e.position);
    const int b = target >= 0 ? target : selected;
    if (b < 0) return;

    const float amount = w.isReversed ? -w.deltaY : w.deltaY;
    const double q = views[(size_t) b].q * std::pow (2.0, amount * (e.mods.isShiftDown() ? 0.5 : 2.5));
    setParam (b, "q", (float) juce::jlimit (0.025, 40.0, q));
    updateBands (false);
}

bool EQDisplay::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && selected >= 0)
    {
        deleteBand (selected);
        return true;
    }
    return false;
}

void EQDisplay::showBandMenu (int b)
{
    const auto v = views[(size_t) b];
    juce::PopupMenu menu, slopeMenu, placementMenu;

    for (int t = 0; t < eq::numFilterTypes; ++t)
        menu.addItem (1 + t, eq::filterTypeNames[t], true, (int) v.type == t);

    for (int s = 0; s < eq::numSlopes; ++s)
        slopeMenu.addItem (100 + s, eq::slopeNames[s], true, v.slope == s);
    for (int p = 0; p < eq::numPlacements; ++p)
        placementMenu.addItem (200 + p, eq::placementNames[p], true, v.placement == p);

    menu.addSeparator();
    menu.addSubMenu ("Slope", slopeMenu, eq::usesSlope (v.type));
    menu.addSubMenu ("Stereo Placement", placementMenu);
    menu.addSeparator();
    menu.addItem (300, "Bypass", true, v.bypass);
    menu.addItem (301, "Delete Band");

    juce::Component::SafePointer<EQDisplay> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safe, b, v] (int r)
                        {
                            if (safe == nullptr || r == 0) return;
                            if (r >= 1 && r <= eq::numFilterTypes)
                            {
                                const auto t = (eq::FilterType) (r - 1);
                                if (eq::usesSlope (t) && ! eq::usesSlope (v.type)) safe->setParam (b, "q", 0.7071f);
                                safe->setParam (b, "type", (float) (r - 1));
                            }
                            else if (r >= 100 && r < 100 + eq::numSlopes) safe->setParam (b, "slope", (float) (r - 100));
                            else if (r >= 200 && r < 200 + eq::numPlacements) safe->setParam (b, "placement", (float) (r - 200));
                            else if (r == 300) safe->setParam (b, "bypass", v.bypass ? 0.0f : 1.0f);
                            else if (r == 301) safe->deleteBand (b);
                        });
}

//==============================================================================
// Painting
void EQDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth(), h = bounds.getHeight();

    g.setGradientFill (juce::ColourGradient (theme::backgroundHi, 0, 0, theme::background, 0, h, false));
    g.fillRect (bounds);

    // --- Frequency grid
    g.setFont (theme::font (11.0f));
    for (double decade = 10.0; decade < maxFreq; decade *= 10.0)
    {
        for (int m = 1; m <= 9; ++m)
        {
            const double f = decade * m;
            if (f <= minFreq || f >= maxFreq) continue;
            const float x = freqToX (f);
            g.setColour (juce::Colours::white.withAlpha (m == 1 ? 0.09f : 0.035f));
            g.drawVerticalLine ((int) x, 0.0f, h - 22.0f);
        }
    }
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = freqToX (f);
        const auto label = f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f);
        g.setColour (theme::textDim);
        g.drawText (label, juce::Rectangle<float> (x - 20.0f, h - 20.0f, 40.0f, 16.0f), juce::Justification::centred);
    }

    // --- Gain grid
    const float step = range <= 3.0f ? 1.0f : range <= 6.0f ? 2.0f : range <= 12.0f ? 3.0f : 6.0f;
    for (float db = -range; db <= range + 0.01f; db += step)
    {
        const float y = dbToY (db);
        g.setColour (juce::Colours::white.withAlpha (std::abs (db) < 0.01f ? 0.16f : 0.05f));
        g.drawHorizontalLine ((int) y, 0.0f, w);
        g.setColour (theme::textDim);
        const auto label = (db > 0 ? "+" : "") + juce::String ((int) db);
        g.drawText (label, juce::Rectangle<float> (w - 34.0f, y - 8.0f, 28.0f, 16.0f), juce::Justification::centredRight);
    }

    // --- Spectrum analyzer
    auto spectrumPath = [&] (const std::vector<float>& pix)
    {
        juce::Path p;
        p.startNewSubPath (0.0f, h);
        for (size_t k = 0; k < xs.size(); ++k)
            p.lineTo (xs[k], juce::jlimit (0.0f, h, specToY (pix[k])));
        p.lineTo (xs.back(), h);
        p.closeSubPath();
        return p;
    };

    if (! xs.empty() && analyzerMode != 3)
    {
        if (analyzerMode == 0 || analyzerMode == 2)
        {
            auto pre = spectrumPath (prePix);
            g.setColour (juce::Colour (0xff4a5568).withAlpha (analyzerMode == 2 ? 0.45f : 0.22f));
            g.fillPath (pre);
        }
        if (analyzerMode == 0 || analyzerMode == 1)
        {
            auto post = spectrumPath (postPix);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0x5579a8e0), 0, h * 0.3f, juce::Colour (0x10476b9e), 0, h, false));
            g.fillPath (post);
            g.setColour (juce::Colour (0x9093b8e8));
            g.strokePath (post, juce::PathStrokeType (1.0f));
        }
    }

    if (xs.empty()) return;

    auto curvePath = [&] (const std::vector<float>& db)
    {
        juce::Path p;
        for (size_t k = 0; k < xs.size(); ++k)
        {
            const float y = juce::jlimit (-50.0f, h + 50.0f, dbToY (db[k]));
            if (k == 0) p.startNewSubPath (xs[k], y);
            else p.lineTo (xs[k], y);
        }
        return p;
    };

    const float zeroY = dbToY (0.0);

    // --- Individual band curves
    for (int b = 0; b < N; ++b)
    {
        const auto& v = views[(size_t) b];
        if (! v.used) continue;
        const auto col = theme::band (b);
        auto p = curvePath (bandCurves[(size_t) b]);
        const bool focus = b == selected || b == hovered || b == dragBand;

        if (focus)
        {
            juce::Path fill (p);
            fill.lineTo (xs.back(), zeroY);
            fill.lineTo (xs.front(), zeroY);
            fill.closeSubPath();
            g.setColour (col.withAlpha (v.bypass ? 0.06f : 0.16f));
            g.fillPath (fill);
        }
        g.setColour (col.withAlpha (v.bypass ? 0.18f : (focus ? 0.85f : 0.32f)));
        g.strokePath (p, juce::PathStrokeType (focus ? 1.5f : 1.0f));
    }

    // --- Composite curve
    {
        auto p = curvePath (composite);
        g.setColour (theme::curve.withAlpha (0.12f));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (theme::curve);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- Ghost node
    if (ghostVisible())
    {
        const float gy = dbToY (compositeDbAt (mousePos.x));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawEllipse (mousePos.x - 6.0f, gy - 6.0f, 12.0f, 12.0f, 1.5f);
        g.drawLine (mousePos.x - 3.0f, gy, mousePos.x + 3.0f, gy, 1.2f);
        g.drawLine (mousePos.x, gy - 3.0f, mousePos.x, gy + 3.0f, 1.2f);
    }

    // --- Nodes
    static const char* placementTags[] = { "", "L", "R", "M", "S" };
    for (int b = 0; b < N; ++b)
    {
        const auto& v = views[(size_t) b];
        if (! v.used) continue;
        const auto pos = nodePos (b);
        const auto col = theme::band (b);
        const bool sel = b == selected, hot = b == hovered || b == dragBand;
        const float r = (sel || hot) ? 9.0f : 7.5f;

        if (v.bypass)
        {
            g.setColour (theme::background.withAlpha (0.8f));
            g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
            g.setColour (col.withAlpha (0.5f));
            g.drawEllipse (pos.x - r, pos.y - r, r * 2, r * 2, 1.5f);
        }
        else
        {
            if (sel)
            {
                g.setColour (col.withAlpha (0.25f));
                g.fillEllipse (pos.x - r - 5, pos.y - r - 5, (r + 5) * 2, (r + 5) * 2);
            }
            g.setColour (col);
            g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
        }
        if (sel)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (pos.x - r - 2.5f, pos.y - r - 2.5f, (r + 2.5f) * 2, (r + 2.5f) * 2, 1.5f);
        }

        g.setColour (v.bypass ? col.withAlpha (0.7f) : juce::Colour (0xff121316));
        g.setFont (theme::font (10.5f, true));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (pos.x - r, pos.y - r, r * 2, r * 2), juce::Justification::centred);

        if (v.placement != 0)
        {
            g.setColour (col);
            g.setFont (theme::font (10.0f, true));
            g.drawText (placementTags[v.placement], juce::Rectangle<float> (pos.x - 8, pos.y + r + 2, 16, 12), juce::Justification::centred);
        }
    }

    // --- Tooltip for the hovered / dragged node
    const int tip = dragBand >= 0 ? dragBand : hovered;
    if (tip >= 0 && views[(size_t) tip].used)
    {
        const auto& v = views[(size_t) tip];
        juce::String text = textfmt::freq (v.freq);
        if (eq::usesGain (v.type)) text << "   " << textfmt::gain (v.gain);
        text << "   Q " << textfmt::q (v.q);

        g.setFont (theme::font (12.0f));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 18.0f;
        const auto pos = nodePos (tip);
        auto box = juce::Rectangle<float> (pos.x - tw * 0.5f, pos.y - 42.0f, tw, 22.0f);
        if (box.getY() < 2.0f) box.setY (pos.y + 18.0f);
        box = box.withX (juce::jlimit (2.0f, w - tw - 2.0f, box.getX()));

        g.setColour (juce::Colour (0xe0202329));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (theme::band (tip).withAlpha (0.7f));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (theme::text);
        g.drawText (text, box, juce::Justification::centred);
    }
}
