#pragma once
#include <algorithm>
#include <array>

namespace Sapphire
{
    constexpr float AttenuverterLowSensitivityDenom = 10;
    constexpr float UnipolarAdjustVoltsDefault = 5;

    constexpr unsigned CHAOS_MAX_CHANNELS = 16;
    using chaos_signal_array_t = std::array<float, CHAOS_MAX_CHANNELS>;

    enum class ChaoticModulation
    {
        None,
        Monophonic,
        Stereo,
        Polyphonic,
    };

    struct SapphireAttenuverterPersist
    {
        bool lowSensitivityMode = false;
        bool unipolar = false;
        float adjust = UnipolarAdjustVoltsDefault;

        void initialize()
        {
            lowSensitivityMode = false;
            unipolar = false;
            adjust = UnipolarAdjustVoltsDefault;
        }
    };


    struct SapphireAttenuverterContext
    {
        SapphireAttenuverterPersist persist;
        unsigned inputPortId = -1;    // port to check for cables, for chaos display color
        ChaoticModulation chaosOption = ChaoticModulation::None;
        chaos_signal_array_t chaosVoltage{};
        float sensitivity = 1;
        unsigned chaosOffset = -1;        // map attenuverter ID to offset in chaos batch

        void initialize()
        {
            persist.initialize();
            for (float& v : chaosVoltage)
                v = 0;
        }

        float adjustVoltage(float v) const
        {
            return persist.unipolar ? std::max<float>(0, (v*sensitivity) + persist.adjust) : (v*sensitivity);
        }
    };


    struct SapphireAttenuverterRecord
    {
        unsigned attenId = -1;
        SapphireAttenuverterPersist persist;
    };
}
