#pragma once
#include <algorithm>

namespace Sapphire
{
    constexpr float AttenuverterLowSensitivityDenom = 10;
    constexpr float UnipolarAdjustVoltsDefault = 5;

    struct SapphireAttenuverterContext
    {
        bool lowSensitivityMode{};
        bool unipolar{};
        float adjust = UnipolarAdjustVoltsDefault;
        int inputPortId = -1;    // If set to a non-negative value, indicates port to check for cables, for chaos display color
        bool supportsChaos = false;

        void initialize()
        {
            lowSensitivityMode = false;
            unipolar = false;
            adjust = UnipolarAdjustVoltsDefault;
        }

        float adjustVoltage(float v) const
        {
            if (unipolar)
                return std::max<float>(0, v+adjust);
            return v;
        }
    };
}
