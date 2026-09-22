#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

namespace omg
{
    /** Factory bank plus user presets on disk, with favourites.

        Loading a preset writes real parameter values through the APVTS, so the
        host sees the change and automation stays valid.
    */
    class PresetManager
    {
    public:
        struct Entry
        {
            juce::String name, category;
            bool isUser = false;
            juce::File file;
        };

        explicit PresetManager (juce::AudioProcessorValueTreeState& state);

        void rescan();

        int getNumPresets() const                 { return entries.size(); }
        const Entry& getPreset (int index) const  { return entries.getReference (juce::jlimit (0, entries.size() - 1, index)); }
        juce::StringArray getNames() const;

        int  getCurrentIndex() const              { return currentIndex; }
        juce::String getCurrentName() const;

        void load (int index);
        void loadNext()     { load (currentIndex + 1); }
        void loadPrevious() { load (currentIndex - 1); }

        bool saveUser (const juce::String& name);
        bool deleteUser (int index);

        bool isFavourite (int index) const;
        void toggleFavourite (int index);

        void setDirty (bool shouldBeDirty) { dirty = shouldBeDirty; }
        bool isDirty() const               { return dirty; }

        static juce::File getUserPresetDirectory();

        std::function<void()> onChanged;

    private:
        void applySettingsString (const juce::String& settings);
        juce::File favouritesFile() const;
        void loadFavourites();
        void saveFavourites();

        juce::AudioProcessorValueTreeState& apvts;
        juce::Array<Entry> entries;
        juce::StringArray favourites;
        int currentIndex { 0 };
        bool dirty { false };
    };
}
