#include "PluginEditor.h"

using namespace omg;
using namespace omg::ui;

OmgnedEditor::OmgnedEditor (OmgnedProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      topBar (p),
      inputSection (p.apvts, p.inputMeter),
      engineSection (p.apvts, [&p] { return p.getDriveDb(); }),
      outputSection (p.apvts, p.outputMeter, [&p] { return p.getLimiterReductionDb(); }),
      eqPanel (p, p.apvts),
      compressorPanel (p.apvts, [&p] { return p.getCompressorReductionDb(); }),
      deEsserPanel (p.apvts, [&p] { return p.getDeEsserReductionDb(); }),
      macroPanel (p.apvts),
      mixPanel (p.apvts),
      filterFxPanel (p.apvts),
      pitchModPanel (p.apvts),
      reverbPanel (p.apvts),
      delayPanel (p.apvts),
      advancedPanel (p.apvts,
                     [&p] { return p.getLatencySamples(); },
                     [&p] { return p.isSidechainConnected(); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (topBar);
    for (auto* c : mainPanelComponents())
        content.addAndMakeVisible (c);

    rackTabs.setPage ((int) processor.apvts.state.getProperty ("rackTab", 0), false);
    rackTabs.onPageChanged = [this] (int page)
    {
        processor.apvts.state.setProperty ("rackTab", page, nullptr);
        layoutContent();
    };

    advancedViewport.setViewedComponent (&advancedPanel, false);
    advancedViewport.setScrollBarsShown (true, false);
    advancedViewport.setScrollBarThickness (10);
    content.addChildComponent (advancedViewport);

    topBar.onBrowsePresets = [this] { openBrowser(); };
    topBar.onToggleAdvanced = [this] { toggleAdvanced(); };
    topBar.onSettings = [this] { showSettingsMenu(); };
    topBar.onHelp = [this] { showHelp(); };

    constrainer.setSizeLimits (metric::minWidth, metric::minHeight, 2400, 1440);
    setConstrainer (&constrainer);
    setResizable (true, true);

    const auto saved = processor.getSavedEditorSize();
    setSize (saved.getWidth(), saved.getHeight());

    startTimerHz (8);
}

OmgnedEditor::~OmgnedEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

std::vector<juce::Component*> OmgnedEditor::mainPanelComponents()
{
    return { &inputSection, &engineSection, &outputSection, &rackTabs,
             &eqPanel, &compressorPanel, &deEsserPanel,
             &filterFxPanel, &pitchModPanel, &reverbPanel, &delayPanel,
             &macroPanel, &mixPanel };
}

void OmgnedEditor::timerCallback()
{
    const bool powered = processor.apvts.getRawParameterValue (pid::power)->load() > 0.5f;
    if (powered != poweredCache)
    {
        poweredCache = powered;
        content.setAlpha (powered ? 1.0f : 0.55f);
        repaint();
    }
}

void OmgnedEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::chassis000);
}

void OmgnedEditor::resized()
{
    scale = juce::jlimit (0.70f, 2.00f, (float) getWidth() / (float) metric::designWidth);

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0,
                       juce::roundToInt ((float) getWidth() / scale),
                       juce::roundToInt ((float) getHeight() / scale));

    layoutContent();
    processor.setEditorSize (getWidth(), getHeight());
}

void OmgnedEditor::layoutContent()
{
    auto area = content.getLocalBounds();
    const int w = area.getWidth();
    const int h = area.getHeight();

    topBar.setBounds (area.removeFromTop (metric::topBarHeight));
    area.reduce (metric::space3, metric::space3);

    if (advancedVisible)
    {
        advancedViewport.setBounds (area);

        const int innerWidth = area.getWidth() - advancedViewport.getScrollBarThickness();
        advancedPanel.setBounds (0, 0, innerWidth,
                                 juce::jmax (area.getHeight(),
                                             advancedPanel.preferredHeightForWidth (innerWidth)));
        return;
    }

    // ---- lower processing strip ------------------------------------------
    const bool roomForLower = h >= 560;
    const bool roomForDeEsser = w >= 1150;

    juce::Rectangle<int> lower;
    if (roomForLower)
    {
        lower = area.removeFromBottom (juce::jlimit (236, 360, h * 37 / 100));
        area.removeFromBottom (metric::space3);
    }

    // ---- upper row ---------------------------------------------------------
    const int sideWidth = juce::jlimit (150, 210, w / 7);
    const int gutter = w >= 1200 ? metric::space10 : metric::space6;

    inputSection.setBounds (area.removeFromLeft (sideWidth));
    area.removeFromLeft (gutter);
    outputSection.setBounds (area.removeFromRight (sideWidth));
    area.removeFromRight (gutter);
    engineSection.setBounds (area);

    // ---- lower row: the tabbed rack, then MACROS and MIX, always visible ----
    const int page = rackTabs.getPage();
    const std::vector<std::pair<juce::Component*, int>> pageOf {
        { &eqPanel, 0 }, { &compressorPanel, 0 }, { &deEsserPanel, 0 },
        { &filterFxPanel, 1 }, { &pitchModPanel, 1 },
        { &reverbPanel, 2 }, { &delayPanel, 2 } };

    for (auto& [c, pg] : pageOf)
        c->setVisible (roomForLower && pg == page && (c != &deEsserPanel || roomForDeEsser));

    rackTabs.setVisible (roomForLower);
    macroPanel.setVisible (roomForLower);
    mixPanel.setVisible (roomForLower);

    if (! roomForLower)
        return;

    const int gap = metric::space3;
    const float rackWeight = roomForDeEsser ? 4.7f : 3.65f;
    const float unit = ((float) lower.getWidth() - 2.0f * (float) gap) / (rackWeight + 1.3f + 0.85f);

    auto rack = lower.removeFromLeft (juce::roundToInt (unit * rackWeight));
    lower.removeFromLeft (gap);
    macroPanel.setBounds (lower.removeFromLeft (juce::roundToInt (unit * 1.3f)));
    lower.removeFromLeft (gap);
    mixPanel.setBounds (lower);

    rackTabs.setBounds (rack.removeFromTop (24));
    rack.removeFromTop (metric::space2);

    auto lay = [&rack, gap] (std::initializer_list<std::pair<juce::Component*, float>> items)
    {
        float total = 0.0f;
        for (auto& it : items) total += it.second;
        const float u = ((float) rack.getWidth() - (float) gap * (float) (items.size() - 1)) / total;
        auto r = rack;
        int i = 0;
        for (auto& it : items)
        {
            const bool last = ++i == (int) items.size();
            it.first->setBounds (last ? r : r.removeFromLeft (juce::roundToInt (u * it.second)));
            if (! last) r.removeFromLeft (gap);
        }
    };

    if (roomForDeEsser)
        lay ({ { &eqPanel, 2.15f }, { &compressorPanel, 1.5f }, { &deEsserPanel, 1.05f } });
    else
        lay ({ { &eqPanel, 2.15f }, { &compressorPanel, 1.5f } });

    lay ({ { &filterFxPanel, 1.5f }, { &pitchModPanel, 1.0f } });
    lay ({ { &reverbPanel, 1.0f }, { &delayPanel, 1.25f } });
}

void OmgnedEditor::toggleAdvanced()
{
    advancedVisible = ! advancedVisible;
    advancedViewport.setVisible (advancedVisible);
    topBar.setAdvancedActive (advancedVisible);

    for (auto* c : mainPanelComponents())
        c->setVisible (! advancedVisible);

    layoutContent();
}

void OmgnedEditor::openBrowser()
{
    if (browser != nullptr)
    {
        closeBrowser();
        return;
    }

    browser = std::make_unique<PresetBrowserComponent> (processor.presetManager);
    browser->onClose = [this] { closeBrowser(); };
    content.addAndMakeVisible (*browser);

    auto area = content.getLocalBounds().withSizeKeepingCentre (
        juce::jmin (560, content.getWidth() - 80), juce::jmin (420, content.getHeight() - 120));
    browser->setBounds (area);
    browser->grabKeyboardFocus();
}

void OmgnedEditor::closeBrowser()
{
    browser.reset();
}

void OmgnedEditor::setZoom (float factor)
{
    setSize (juce::roundToInt (metric::designWidth * factor),
             juce::roundToInt (metric::designHeight * factor));
}

void OmgnedEditor::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);
    menu.addSectionHeader ("INTERFACE SCALE");
    menu.addItem (1, "75 %");
    menu.addItem (2, "100 %");
    menu.addItem (3, "125 %");
    menu.addItem (4, "150 %");
    menu.addSeparator();
    menu.addItem (5, "Open user preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        switch (r)
        {
            case 1: setZoom (0.75f); break;
            case 2: setZoom (1.00f); break;
            case 3: setZoom (1.25f); break;
            case 4: setZoom (1.50f); break;
            case 5: PresetManager::getUserPresetDirectory().revealToUser(); break;
            default: break;
        }
    });
}

void OmgnedEditor::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::NoIcon, "OMGNEDD",
        "Pick an engine, turn CHARACTER, set MIX. Everything else refines that.\n\n"
        "UNDERWATER, DISTORTION and SATURATION each replace the centre controls. "
        "The lower rack has three pages: TONE (EQ, compressor, de-esser), FX (wah, wobble, talk, phaser, "
        "chorus and the pitch layer) and SPACE (reverb and delay). The six vocal macros and the final mix are always there.\n\n"
        "Every knob: drag to change, shift-drag for fine, wheel to nudge, double-click to reset, "
        "right-click to type a value or copy it to another control. Every control has a tooltip.\n\n"
        "ADV opens the engineering panel. A and B hold two complete states; the arrows copy one into the other.",
        "Close");
}
