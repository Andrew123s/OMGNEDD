#include "PresetManager.h"
#include "../Parameters.h"

namespace omg
{
    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        loadFavourites();
        rescan();
    }

    juce::File PresetManager::getUserPresetDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("OMGNEDD").getChildFile ("Presets");
        if (! dir.exists())
            dir.createDirectory();
        return dir;
    }

    juce::File PresetManager::favouritesFile() const
    {
        return getUserPresetDirectory().getParentDirectory().getChildFile ("favourites.txt");
    }

    void PresetManager::loadFavourites()
    {
        favourites.clear();
        auto f = favouritesFile();
        if (f.existsAsFile())
            favourites.addLines (f.loadFileAsString());
        favourites.removeEmptyStrings();
    }

    void PresetManager::saveFavourites()
    {
        favouritesFile().replaceWithText (favourites.joinIntoString ("\n"));
    }

    void PresetManager::rescan()
    {
        const auto previous = getCurrentName();
        entries.clearQuick();

        for (int i = 0; i < numFactoryPresets(); ++i)
        {
            const auto& p = factoryPresets()[i];
            entries.add ({ juce::String (p.name), juce::String (p.category), false, {} });
        }

        for (const auto& file : getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.omgnedd"))
            entries.add ({ file.getFileNameWithoutExtension(), "USER", true, file });

        currentIndex = juce::jmax (0, getNames().indexOf (previous));

        if (onChanged != nullptr)
            onChanged();
    }

    juce::StringArray PresetManager::getNames() const
    {
        juce::StringArray names;
        for (const auto& e : entries)
            names.add (e.name);
        return names;
    }

    juce::String PresetManager::getCurrentName() const
    {
        if (! juce::isPositiveAndBelow (currentIndex, entries.size()))
            return "INIT";
        return entries.getReference (currentIndex).name + (dirty ? "*" : "");
    }

    void PresetManager::applySettingsString (const juce::String& settings)
    {
        // start from the defaults so a preset is a complete state, not a diff on the last one
        for (int i = 0; i < numParams(); ++i)
        {
            const auto& d = params()[i];
            if (auto* p = apvts.getParameter (d.id))
                p->setValueNotifyingHost (p->convertTo0to1 (d.def));
        }

        for (const auto& pair : juce::StringArray::fromTokens (settings, ";", ""))
        {
            const auto trimmed = pair.trim();
            if (trimmed.isEmpty()) continue;

            const auto id = trimmed.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = trimmed.fromFirstOccurrenceOf ("=", false, false).trim().getFloatValue();

            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        }
    }

    void PresetManager::load (int index)
    {
        if (entries.isEmpty()) return;

        currentIndex = (index % entries.size() + entries.size()) % entries.size();
        const auto& e = entries.getReference (currentIndex);

        if (e.isUser)
        {
            if (auto xml = juce::XmlDocument::parse (e.file))
            {
                auto tree = juce::ValueTree::fromXml (*xml);
                if (tree.isValid())
                    apvts.replaceState (tree);
            }
        }
        else
        {
            applySettingsString (juce::String (factoryPresets()[currentIndex].settings));
        }

        dirty = false;

        if (onChanged != nullptr)
            onChanged();
    }

    bool PresetManager::saveUser (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;

        auto file = getUserPresetDirectory().getChildFile (clean + ".omgnedd");

        if (auto xml = apvts.copyState().createXml())
        {
            const bool ok = file.replaceWithText (xml->toString());
            rescan();
            currentIndex = juce::jmax (0, getNames().indexOf (clean));
            dirty = false;
            if (onChanged != nullptr) onChanged();
            return ok;
        }

        return false;
    }

    bool PresetManager::deleteUser (int index)
    {
        if (! juce::isPositiveAndBelow (index, entries.size())) return false;

        const auto& e = entries.getReference (index);
        if (! e.isUser) return false;

        const bool ok = e.file.deleteFile();
        rescan();
        return ok;
    }

    bool PresetManager::isFavourite (int index) const
    {
        if (! juce::isPositiveAndBelow (index, entries.size())) return false;
        return favourites.contains (entries.getReference (index).name);
    }

    void PresetManager::toggleFavourite (int index)
    {
        if (! juce::isPositiveAndBelow (index, entries.size())) return;

        const auto name = entries.getReference (index).name;
        if (favourites.contains (name))
            favourites.removeString (name);
        else
            favourites.add (name);

        saveFavourites();
        if (onChanged != nullptr) onChanged();
    }
}
