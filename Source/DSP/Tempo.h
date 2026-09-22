#pragma once
#include <cmath>

namespace omg::dsp
{
    /** The note divisions every tempo-synced control shares, in the order the
        choice parameters list them. Values are in quarter-note beats. */
    struct Divisions
    {
        static constexpr const char* names = "1/1|1/2|1/2D|1/2T|1/4|1/4D|1/4T|1/8|1/8D|1/8T|1/16|1/16D|1/16T|1/32";
        static constexpr int count = 14;

        static double beats (int index)
        {
            static const double b[] = { 4.0, 2.0, 3.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0,
                                        0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };
            return b[index < 0 ? 0 : index >= count ? count - 1 : index];
        }

        static double seconds (int index, double bpm)
        {
            return beats (index) * 60.0 / (bpm > 1.0 ? bpm : 120.0);
        }
    };

    /** What the host told us about time this block. */
    struct TransportInfo
    {
        double bpm { 120.0 };
        double ppqAtBlockStart { 0.0 };
        bool   playing { false };
    };
}
