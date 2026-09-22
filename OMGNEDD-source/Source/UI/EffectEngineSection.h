#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "ModeSelector.h"
#include "OmgButton.h"

namespace omg::ui
{
    /** One page of engine controls: the knobs belonging to that engine, and,
        where the engine has one, its algorithm selector.
    */
    class EnginePage : public juce::Component
    {
    public:
        EnginePage (juce::AudioProcessorValueTreeState& state,
                    const juce::StringArray& knobIDs,
                    juce::StringRef algorithmParam,
                    juce::String algorithmCaption)
        {
            for (const auto& id : knobIDs)
            {
                auto* k = knobs.add (new OmgKnob (state, id, OmgKnob::Small,
                                                  id == pid::satTone || id == pid::dsBias || id == pid::dsAsym));
                addAndMakeVisible (k);
            }

            if (algorithmParam.isNotEmpty())
            {
                selector = std::make_unique<OmgSelector> (state, algorithmParam, 2, false);
                caption = std::make_unique<OmgCaption> (algorithmCaption);
                addAndMakeVisible (*selector);
                addAndMakeVisible (*caption);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();

            if (selector != nullptr)
            {
                auto right = area.removeFromRight (juce::jlimit (130, 210, area.getWidth() / 3));
                caption->setBounds (right.removeFromTop (13));
                selector->setBounds (right);
                area.removeFromRight (metric::space4);
            }

            if (knobs.isEmpty()) return;

            const int rows = area.getHeight() >= 172 ? 2 : 1;
            const int perRow = (knobs.size() + rows - 1) / rows;
            const int cellW = area.getWidth() / juce::jmax (1, perRow);
            const int cellH = area.getHeight() / rows;

            for (int i = 0; i < knobs.size(); ++i)
                knobs[i]->setBounds (area.getX() + (i % perRow) * cellW,
                                     area.getY() + (i / perRow) * cellH, cellW, cellH);
        }

    private:
        juce::OwnedArray<OmgKnob> knobs;
        std::unique_ptr<OmgSelector> selector;
        std::unique_ptr<OmgCaption> caption;
    };

    /** The centre of the interface: mode selector, CHARACTER with the engine's
        own MIX beside it, and the controls belonging to the selected engine.
    */
    class EffectEngineSection : public OmgPanel,
                                private juce::Timer
    {
    public:
        explicit EffectEngineSection (juce::AudioProcessorValueTreeState& state)
            : OmgPanel ({}, true),
              apvts (state),
              modes (state, pid::engine, 3, true),
              character (state, pid::character, OmgKnob::Large)
        {
            addAndMakeVisible (modes);
            addAndMakeVisible (character);

            pages.add (new EnginePage (state,
                { pid::uwDepth, pid::uwWater, pid::uwMurk, pid::uwPressure,
                  pid::uwRipple, pid::uwWave, pid::uwBubble }, {}, {}));

            pages.add (new EnginePage (state,
                { pid::dsDrive, pid::dsBite, pid::dsBody,
                  pid::dsCrush, pid::dsEdge, pid::dsSmooth },
                pid::dsType, "DISTORTION TYPE"));

            pages.add (new EnginePage (state,
                { pid::satDrive, pid::satWarmth, pid::satHarmonics,
                  pid::satThickness, pid::satTone, pid::satSoftClip },
                pid::satModel, "CHARACTER MODEL"));

            for (auto id : { pid::uwMix, pid::dsMix, pid::satMix })
            {
                auto* k = mixKnobs.add (new OmgKnob (state, id, OmgKnob::Medium));
                k->setLabelText ("MIX");
                addChildComponent (k);
            }

            for (auto* p : pages)
                addChildComponent (p);

            startTimerHz (15);
            updatePage();
        }

        ~EffectEngineSection() override { stopTimer(); }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getLocalBounds().reduced (metric::space4);

            modes.setBounds (area.removeFromTop (juce::jlimit (34, 46, area.getHeight() / 8)));
            area.removeFromTop (metric::space3);

            const int pageHeight = juce::jlimit (92, 200, area.getHeight() / 3);
            auto pageArea = area.removeFromBottom (pageHeight);
            area.removeFromBottom (metric::space3);

            // CHARACTER with the engine's own MIX beside it, the pair centred
            const int charW = juce::jmin (200, area.getWidth() / 2);
            const int mixW = juce::jmin (120, area.getWidth() / 4);
            auto pair = area.withSizeKeepingCentre (charW + metric::space6 + mixW, area.getHeight());

            character.setBounds (pair.removeFromLeft (charW));
            pair.removeFromLeft (metric::space6);

            for (auto* k : mixKnobs)
                k->setBounds (pair);

            for (auto* p : pages)
                p->setBounds (pageArea);
        }

    private:
        void timerCallback() override { updatePage(); }

        void updatePage()
        {
            int index = 0;
            if (auto* v = apvts.getRawParameterValue (pid::engine))
                index = juce::jlimit (0, pages.size() - 1, (int) v->load());

            if (index == visiblePage) return;

            visiblePage = index;
            for (int i = 0; i < pages.size(); ++i)
                pages[i]->setVisible (i == index);
            for (int i = 0; i < mixKnobs.size(); ++i)
                mixKnobs[i]->setVisible (i == index);
        }

        juce::AudioProcessorValueTreeState& apvts;
        OmgSelector modes;
        OmgKnob character;
        juce::OwnedArray<OmgKnob> mixKnobs;
        juce::OwnedArray<EnginePage> pages;
        int visiblePage { -1 };
    };
}
