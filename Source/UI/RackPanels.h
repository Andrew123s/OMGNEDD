#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgButton.h"
#include "ModeSelector.h"

namespace omg::ui
{
    /** Lays knobs out in a grid of @p perRow, as many rows as fit. */
    inline void layoutKnobGrid (juce::Rectangle<int> area, juce::OwnedArray<OmgKnob>& knobs, int perRow)
    {
        if (knobs.isEmpty()) return;
        const int rows = (knobs.size() + perRow - 1) / perRow;
        const int cellW = area.getWidth() / juce::jmax (1, perRow);
        const int cellH = juce::jmax (1, area.getHeight() / rows);
        for (int i = 0; i < knobs.size(); ++i)
            knobs[i]->setBounds (area.getX() + (i % perRow) * cellW, area.getY() + (i / perRow) * cellH, cellW, cellH);
    }

    inline OmgKnob* makeKnob (juce::Component& parent, juce::OwnedArray<OmgKnob>& into,
                              juce::AudioProcessorValueTreeState& s, const char* id, const char* legend, bool bipolar = false)
    {
        auto* k = into.add (new OmgKnob (s, id, OmgKnob::Small, bipolar));
        k->setLabelText (legend);
        parent.addAndMakeVisible (k);
        return k;
    }

    /** A panel whose header carries a latching ON button. */
    class SwitchedPanel : public OmgPanel,
                          protected juce::Timer
    {
    public:
        SwitchedPanel (juce::AudioProcessorValueTreeState& s, const juce::String& title, const char* onParam, const char* onLegend)
            : OmgPanel (title), apvts (s), onButton (s, onParam)
        {
            onButton.setLegend (onLegend);
            onButton.setCompact (true);
            addAndMakeVisible (onButton);
            setHeaderRight (&onButton);
            startTimerHz (8);
        }

        ~SwitchedPanel() override { stopTimer(); }

    protected:
        bool isOn (const char* id) const
        {
            if (auto* v = apvts.getRawParameterValue (id)) return v->load() > 0.5f;
            return false;
        }

        juce::AudioProcessorValueTreeState& apvts;
        OmgButton onButton;
        juce::OwnedArray<OmgKnob> knobs;
    };

    // -------------------------------------------------------------------------
    /** Wah, wobble, talk and phaser. */
    class FilterFxPanel : public SwitchedPanel
    {
    public:
        explicit FilterFxPanel (juce::AudioProcessorValueTreeState& s)
            : SwitchedPanel (s, "FILTER FX", pid::fxOn, "FX ON"),
              modes (s, pid::fxMode, 5, false),
              sync (s, pid::fxSync),
              div (s, pid::fxDiv, "SYNC RATE"),
              shape (s, pid::fxShape, "SHAPE")
        {
            addAndMakeVisible (modes);
            makeKnob (*this, knobs, s, pid::fxFreq,   "FREQ");
            makeKnob (*this, knobs, s, pid::fxDepth,  "DEPTH");
            makeKnob (*this, knobs, s, pid::fxReso,   "RESO");
            makeKnob (*this, knobs, s, pid::fxSens,   "SENS");
            makeKnob (*this, knobs, s, pid::fxDrive,  "DRIVE");
            makeKnob (*this, knobs, s, pid::fxStereo, "STEREO");
            makeKnob (*this, knobs, s, pid::fxMix,    "MIX");
            rate = makeKnob (*this, rateKnob, s, pid::fxRate, "RATE");

            sync.setLegend ("SYNC");
            sync.setCompact (true);
            addAndMakeVisible (sync);
            addAndMakeVisible (div);
            addAndMakeVisible (shape);
            timerCallback();
        }

        void resized() override
        {
            OmgPanel::resized();
            auto a = getContentArea();

            modes.setBounds (a.removeFromTop (modes.getPreferredHeight()));
            a.removeFromTop (metric::space2);

            auto side = a.removeFromRight (juce::jlimit (96, 130, a.getWidth() / 4));
            a.removeFromRight (metric::space2);

            // SYNC, then one slot shared by the note division and the free
            // rate knob (only one is shown at a time), then the LFO shape
            sync.setBounds (side.removeFromTop (24));
            side.removeFromTop (metric::space1);
            auto slot = side.removeFromTop (juce::jmax (40, juce::jmin (70, side.getHeight() - 44)));
            div.setBounds (slot.withSizeKeepingCentre (slot.getWidth(), juce::jmin (40, slot.getHeight())));
            rate->setBounds (slot);
            side.removeFromTop (metric::space1);
            shape.setBounds (side.removeFromTop (juce::jmin (side.getHeight(), 40)));

            layoutKnobGrid (a, knobs, a.getHeight() >= 150 ? 4 : 7);
        }

    private:
        void timerCallback() override
        {
            const bool synced = isOn (pid::fxSync);
            div.setVisible (synced);
            rate->setVisible (! synced);
        }

        OmgSelector modes;
        OmgButton sync;
        OmgChoiceButton div, shape;
        juce::OwnedArray<OmgKnob> rateKnob;
        OmgKnob* rate { nullptr };
    };

    // -------------------------------------------------------------------------
    /** Chorus, micro pitch, vibrato and tape, plus the pitch layer. */
    class PitchModPanel : public SwitchedPanel
    {
    public:
        explicit PitchModPanel (juce::AudioProcessorValueTreeState& s)
            : SwitchedPanel (s, "PITCH / MOD", pid::modOn, "MOD ON"),
              mode (s, pid::modMode, "MOD MODE")
        {
            addAndMakeVisible (mode);
            makeKnob (*this, knobs, s, pid::modRate,   "RATE");
            makeKnob (*this, knobs, s, pid::modDepth,  "DEPTH");
            makeKnob (*this, knobs, s, pid::modDetune, "DETUNE");
            makeKnob (*this, knobs, s, pid::modMix,    "MIX");
            makeKnob (*this, knobs, s, pid::modWidth,  "WIDTH");
            makeKnob (*this, knobs, s, pid::modMotion, "MOTION");
            makeKnob (*this, knobs, s, pid::pitShift,  "SHIFT", true);
            makeKnob (*this, knobs, s, pid::pitMix,    "SHIFT MIX");
        }

        void resized() override
        {
            OmgPanel::resized();
            auto a = getContentArea();
            mode.setBounds (a.removeFromTop (36).withTrimmedRight (a.getWidth() / 2));
            a.removeFromTop (metric::space2);
            layoutKnobGrid (a, knobs, 4);
        }

    private:
        void timerCallback() override {}
        OmgChoiceButton mode;
    };

    // -------------------------------------------------------------------------
    class ReverbPanel : public SwitchedPanel
    {
    public:
        explicit ReverbPanel (juce::AudioProcessorValueTreeState& s)
            : SwitchedPanel (s, "REVERB", pid::revOn, "REVERB ON")
        {
            makeKnob (*this, knobs, s, pid::revMix,   "MIX");
            makeKnob (*this, knobs, s, pid::revSize,  "SIZE");
            makeKnob (*this, knobs, s, pid::revDecay, "DECAY");
            makeKnob (*this, knobs, s, pid::revDamp,  "DAMP");
            makeKnob (*this, knobs, s, pid::revPre,   "PRE-DELAY");
            makeKnob (*this, knobs, s, pid::revDuck,  "DUCK");
        }

        void resized() override
        {
            OmgPanel::resized();
            layoutKnobGrid (getContentArea(), knobs, 3);
        }

    private:
        void timerCallback() override {}
    };

    // -------------------------------------------------------------------------
    class DelayPanel : public SwitchedPanel
    {
    public:
        explicit DelayPanel (juce::AudioProcessorValueTreeState& s)
            : SwitchedPanel (s, "DELAY", pid::dlyOn, "DELAY ON"),
              sync (s, pid::dlySync),
              ping (s, pid::dlyPing),
              div (s, pid::dlyDiv, "SYNC TIME")
        {
            makeKnob (*this, knobs, s, pid::dlyMix,      "MIX");
            makeKnob (*this, knobs, s, pid::dlyFeedback, "FEEDBACK");
            makeKnob (*this, knobs, s, pid::dlyTone,     "TONE");
            makeKnob (*this, knobs, s, pid::dlyWarp,     "WARP");
            makeKnob (*this, knobs, s, pid::dlyDuck,     "DUCK");
            time = makeKnob (*this, timeKnob, s, pid::dlyTime, "TIME");

            sync.setLegend ("SYNC");  sync.setCompact (true);
            ping.setLegend ("PING PONG"); ping.setCompact (true);
            addAndMakeVisible (sync);
            addAndMakeVisible (ping);
            addAndMakeVisible (div);
            timerCallback();
        }

        void resized() override
        {
            OmgPanel::resized();
            auto a = getContentArea();

            auto side = a.removeFromRight (juce::jlimit (92, 120, a.getWidth() / 4));
            a.removeFromRight (metric::space2);
            sync.setBounds (side.removeFromTop (24));
            side.removeFromTop (metric::space1);
            ping.setBounds (side.removeFromTop (24));
            side.removeFromTop (metric::space2);
            auto slot = side.removeFromTop (juce::jmax (40, juce::jmin (70, side.getHeight())));
            div.setBounds (slot.withSizeKeepingCentre (slot.getWidth(), juce::jmin (40, slot.getHeight())));
            time->setBounds (slot);

            layoutKnobGrid (a, knobs, a.getHeight() >= 150 ? 3 : 5);
        }

    private:
        void timerCallback() override
        {
            const bool synced = isOn (pid::dlySync);
            div.setVisible (synced);
            time->setVisible (! synced);
        }

        OmgButton sync, ping;
        OmgChoiceButton div;
        juce::OwnedArray<OmgKnob> timeKnob;
        OmgKnob* time { nullptr };
    };

    // -------------------------------------------------------------------------
    /** Three latching tab buttons over the lower rack. */
    class RackTabs : public juce::Component
    {
    public:
        enum Page { Tone = 0, Fx, Space };

        RackTabs()
        {
            const char* names[] = { "TONE", "FX", "SPACE" };
            const char* tips[] = {
                "TONE\nThe equaliser, the compressor and the de-esser.",
                "FX\nWah, wobble, talk and phaser; chorus, vibrato and tape; and the pitch-shifted layer.",
                "SPACE\nReverb and delay, both able to duck under the vocal." };

            for (int i = 0; i < 3; ++i)
            {
                auto* b = buttons.add (new OmgButton (names[i], [this, i] { setPage (i, true); }));
                b->setShowLed (true);
                b->setCompact (true);
                b->setTooltip (tips[i]);
                b->setComponentID (juce::String ("tab") + names[i]);
                addAndMakeVisible (b);
            }
            setPage (Tone, false);
        }

        std::function<void (int)> onPageChanged;

        void setPage (int newPage, bool notify)
        {
            page = juce::jlimit (0, 2, newPage);
            for (int i = 0; i < buttons.size(); ++i) buttons[i]->setManualOn (i == page);
            if (notify && onPageChanged != nullptr) onPageChanged (page);
        }

        int getPage() const { return page; }

        void resized() override
        {
            auto a = getLocalBounds();
            for (auto* b : buttons)
            {
                b->setBounds (a.removeFromLeft (84));
                a.removeFromLeft (metric::space1);
            }
        }

    private:
        juce::OwnedArray<OmgButton> buttons;
        int page { Tone };
    };
}
