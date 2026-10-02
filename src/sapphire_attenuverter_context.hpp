#pragma once
#include <algorithm>
#include <array>

namespace Sapphire
{
    constexpr float AttenuverterLowSensitivityDenom = 10;
    constexpr float UnipolarAdjustVoltsDefault = 5;

    constexpr unsigned CHAOS_MAX_CHANNELS = 16;
    using chaos_signal_array_t = std::array<float, CHAOS_MAX_CHANNELS>;

    struct SapphireAttenuverterContext
    {
        bool lowSensitivityMode{};
        bool unipolar{};
        float adjust = UnipolarAdjustVoltsDefault;
        unsigned inputPortId = -1;    // port to check for cables, for chaos display color
        bool supportsChaos = false;
        chaos_signal_array_t chaosVoltage{};
        float sensitivity = 1;
        unsigned chaosOffset = -1;        // map attenuverter ID to offset in chaos batch

        void initialize()
        {
            lowSensitivityMode = false;
            unipolar = false;
            adjust = UnipolarAdjustVoltsDefault;
            for (unsigned c=0; c < CHAOS_MAX_CHANNELS; ++c)
                chaosVoltage[c] = 0;
        }

        float adjustVoltage(float v) const
        {
            return unipolar ? std::max<float>(0, (v*sensitivity) + adjust) : (v*sensitivity);
        }
    };
}
