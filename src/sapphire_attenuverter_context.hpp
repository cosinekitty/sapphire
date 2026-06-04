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
        unsigned inputPortId = -1;    // port to check for cables, for chaos display color
        bool supportsChaos = false;
        float chaosVoltage[2]{};     // The most recent chaotic voltage (or stereo pair) if choas is active, otherwise 0.

        void initialize()
        {
            lowSensitivityMode = false;
            unipolar = false;
            adjust = UnipolarAdjustVoltsDefault;
            chaosVoltage[0] = chaosVoltage[1] = 0;
        }

        float adjustVoltage(float v) const
        {
            return unipolar ? std::max<float>(0, v+adjust) : v;
        }
    };
}
