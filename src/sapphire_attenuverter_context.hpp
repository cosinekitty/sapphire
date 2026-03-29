#pragma once
#include <algorithm>

namespace Sapphire
{
    constexpr float AttenuverterLowSensitivityDenom = 10;
    constexpr float UnipolarAdjustVoltsDefault = 5;

    struct SapphireAttenuverterContext
    {
        bool lowSensitivityMode{};
        bool unipolar{};    // voltage --> max(0, voltage+5)
        float adjust = UnipolarAdjustVoltsDefault;

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
