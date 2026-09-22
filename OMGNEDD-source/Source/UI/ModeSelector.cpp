#include "ModeSelector.h"
#include "../Parameters.h"

namespace omg::ui
{
    OmgSelector::OmgSelector (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID,
                              int columnsToUse, bool tallButtons)
        : apvts (state), paramID (parameterID), columns (columnsToUse), tall (tallButtons)
    {
        const auto* desc = findParam (parameterID);
        jassert (desc != nullptr && desc->type == PType::Choice);

        parameter = apvts.getParameter (paramID);
        jassert (parameter != nullptr);

        auto choices = juce::StringArray::fromTokens (desc != nullptr ? juce::String (desc->choices) : juce::String(), "|", "");

        for (int i = 0; i < choices.size(); ++i)
        {
            auto* b = buttons.add (new OmgButton (choices[i], [this, i] { setIndex (i); }));
            b->setShowLed (true);
            b->setCompact (! tall);
            b->setLeftAligned (tall);
            if (desc != nullptr)
                b->setTooltip (juce::String (desc->name).toUpperCase() + "\n" + desc->tip + "\nCurrent: " + choices[i]);
            addAndMakeVisible (b);
        }

        if (columns <= 0)
            columns = choices.size() <= 4 ? choices.size() : 4;

        if (parameter != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*parameter,
                                                                      [this] (float) { refresh(); },
                                                                      nullptr);
            attachment->sendInitialUpdate();
            ControlRegistry::note (paramID);
        }

        refresh();
    }

    OmgSelector::~OmgSelector() = default;

    void OmgSelector::setIndex (int index)
    {
        if (parameter == nullptr || attachment == nullptr) return;

        const auto norm = parameter->convertTo0to1 ((float) index);
        attachment->setValueAsCompleteGesture (norm);
        refresh();
    }

    void OmgSelector::refresh()
    {
        if (parameter == nullptr) return;

        const int current = (int) std::round (parameter->convertFrom0to1 (parameter->getValue()));

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setManualOn (i == current);
    }

    int OmgSelector::getPreferredHeight() const
    {
        const int rows = (buttons.size() + columns - 1) / juce::jmax (1, columns);
        const int h = tall ? 40 : 26;
        return rows * h + (rows - 1) * metric::space2;
    }

    void OmgSelector::resized()
    {
        if (buttons.isEmpty()) return;

        const int rows = (buttons.size() + columns - 1) / juce::jmax (1, columns);
        const int gap = metric::space2;
        const int cellW = (getWidth() - (columns - 1) * gap) / juce::jmax (1, columns);
        const int cellH = (getHeight() - (rows - 1) * gap) / juce::jmax (1, rows);

        for (int i = 0; i < buttons.size(); ++i)
        {
            const int r = i / columns, c = i % columns;
            buttons[i]->setBounds (c * (cellW + gap), r * (cellH + gap), cellW, cellH);
        }
    }
}

namespace omg::ui
{
    OmgChoiceButton::OmgChoiceButton (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID,
                                      juce::String captionText)
        : apvts (state), paramID (parameterID), caption (std::move (captionText))
    {
        const auto* desc = findParam (parameterID);
        jassert (desc != nullptr && desc->type == PType::Choice);

        choices = juce::StringArray::fromTokens (desc != nullptr ? juce::String (desc->choices) : juce::String(), "|", "");
        parameter = apvts.getParameter (paramID);

        if (caption.isEmpty() && desc != nullptr)
            caption = juce::String (desc->name).toUpperCase();

        button = std::make_unique<OmgButton> ("", [this]
        {
            juce::PopupMenu menu;
            const int current = parameter != nullptr
                                  ? (int) std::round (parameter->convertFrom0to1 (parameter->getValue())) : 0;

            for (int i = 0; i < choices.size(); ++i)
                menu.addItem (i + 1, choices[i], true, i == current);

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (button.get()),
                                [this] (int r)
            {
                if (r > 0 && attachment != nullptr && parameter != nullptr)
                    attachment->setValueAsCompleteGesture (parameter->convertTo0to1 ((float) (r - 1)));
            });
        });

        button->setShowLed (false);
        button->setCompact (true);
        if (desc != nullptr)
            button->setTooltip (juce::String (desc->name).toUpperCase() + "\n" + desc->tip);

        addAndMakeVisible (*button);

        if (parameter != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float) { refresh(); }, nullptr);
            attachment->sendInitialUpdate();
            ControlRegistry::note (paramID);
        }

        refresh();
    }

    OmgChoiceButton::~OmgChoiceButton() = default;

    void OmgChoiceButton::refresh()
    {
        if (parameter == nullptr || button == nullptr) return;

        const int current = juce::jlimit (0, juce::jmax (0, choices.size() - 1),
                                          (int) std::round (parameter->convertFrom0to1 (parameter->getValue())));
        button->setLegend (choices[current]);
    }

    void OmgChoiceButton::resized()
    {
        auto area = getLocalBounds();
        if (caption.isNotEmpty())
            area.removeFromTop (12);
        button->setBounds (area);
    }

    void OmgChoiceButton::paint (juce::Graphics& g)
    {
        if (caption.isNotEmpty())
            Fonts::drawLabel (g, caption, getLocalBounds().removeFromTop (12), 9.0f, col::inkMuted, 0.12f,
                              juce::Justification::centredLeft, true);
    }
}
