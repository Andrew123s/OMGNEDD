#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/OmgLookAndFeel.h"
#include "UI/TopBar.h"
#include "UI/InputSection.h"
#include "UI/OutputSection.h"
#include "UI/EffectEngineSection.h"
#include "UI/EqPanel.h"
#include "UI/DynamicsPanels.h"
#include "UI/MacroPanel.h"
#include "UI/AdvancedPanel.h"
#include "UI/PresetBrowserComponent.h"

/** The whole interface, laid out in design units and scaled to whatever size the
    host gives it. Nothing here owns a control: the sections do.
*/
class OmgnedEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit OmgnedEditor (OmgnedProcessor&);
    ~OmgnedEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutContent();
    void showSettingsMenu();
    void showHelp();
    void setZoom (float factor);
    void toggleAdvanced();
    void openBrowser();
    void closeBrowser();

    OmgnedProcessor& processor;
    omg::ui::OmgLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    /** Everything lives inside this, which is scaled as one piece. */
    juce::Component content;

    omg::ui::TopBar topBar;
    omg::ui::InputSection inputSection;
    omg::ui::EffectEngineSection engineSection;
    omg::ui::OutputSection outputSection;
    omg::ui::EqPanel eqPanel;
    omg::ui::CompressorPanel compressorPanel;
    omg::ui::DeEsserPanel deEsserPanel;
    omg::ui::MacroPanel macroPanel;
    omg::ui::MixPanel mixPanel;
    omg::ui::AdvancedPanel advancedPanel;
    /** The advanced panel is taller than the window at small sizes, so it lives
        in a viewport: every group stays reachable rather than being clipped. */
    juce::Viewport advancedViewport;
    std::unique_ptr<omg::ui::PresetBrowserComponent> browser;

    juce::ComponentBoundsConstrainer constrainer;
    float scale { 1.0f };
    bool advancedVisible { false };
    bool poweredCache { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgnedEditor)
};
