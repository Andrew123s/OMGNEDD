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
      advancedPanel (p.apvts,
                     [&p] { return p.getLatencySamples(); },
                     [&p] { return p.isSidechainConnected(); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> {
            &topBar, &inputSection, &engineSection, &outputSection,
            &eqPanel, &compressorPanel, &deEsserPanel, &macroPanel, &mixPanel })
        content.addAndMakeVisible (c);

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
        lower = area.removeFromBottom (juce::jlimit (210, 340, h * 34 / 100));
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

    // ---- lower row ------------------------------------------------------------
    eqPanel.setVisible (roomForLower);
    compressorPanel.setVisible (roomForLower);
    deEsserPanel.setVisible (roomForLower && roomForDeEsser);
    macroPanel.setVisible (roomForLower);
    mixPanel.setVisible (roomForLower);

    if (! roomForLower)
        return;

    const int gap = metric::space3;
    // weights: EQ gets the space a display needs, MIX the least
    const float weights = roomForDeEsser ? 2.15f + 1.5f + 1.05f + 1.3f + 0.85f
                                         : 2.15f + 1.5f + 1.3f + 0.85f;
    const float unit = ((float) lower.getWidth() - (roomForDeEsser ? 4 : 3) * (float) gap) / weights;
    auto take = [&lower, gap] (float width)
    {
        auto r = lower.removeFromLeft (juce::roundToInt (width));
        lower.removeFromLeft (gap);
        return r;
    };

    eqPanel.setBounds (take (unit * 2.15f));
    compressorPanel.setBounds (take (unit * 1.5f));

    if (roomForDeEsser)
        deEsserPanel.setBounds (take (unit * 1.05f));

    macroPanel.setBounds (take (unit * 1.3f));
    mixPanel.setBounds (lower);
}

void OmgnedEditor::toggleAdvanced()
{
    advancedVisible = ! advancedVisible;
    advancedViewport.setVisible (advancedVisible);
    topBar.setAdvancedActive (advancedVisible);

    for (auto* c : std::initializer_list<juce::Component*> {
            &inputSection, &engineSection, &outputSection,
            &eqPanel, &compressorPanel, &deEsserPanel, &macroPanel, &mixPanel })
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
        "The lower strip is always live: EQ, compressor, de-esser, the six vocal macros and the final mix.\n\n"
        "Every knob: drag to change, shift-drag for fine, wheel to nudge, double-click to reset, "
        "right-click to type a value or copy it to another control. Every control has a tooltip.\n\n"
        "ADV opens the engineering panel. A and B hold two complete states; the arrows copy one into the other.",
        "Close");
}
