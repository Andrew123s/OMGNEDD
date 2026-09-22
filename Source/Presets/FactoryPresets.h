#pragma once
#include <juce_core/juce_core.h>

namespace omg
{
    struct FactoryPreset
    {
        const char* name;
        const char* category;   // UNDERWATER | DISTORTION | SATURATION | VOCAL | EXTREME | HYBRID
        const char* settings;   // "id=value;id=value" in plain units; anything absent keeps its default
    };

    const FactoryPreset* factoryPresets();
    int                  numFactoryPresets();
}
