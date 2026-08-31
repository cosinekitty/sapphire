#pragma once
#include <array>
#include <algorithm>
#include "sapphire_gate_trigger.hpp"

namespace Sapphire
{
    constexpr unsigned NSTEREO = 2;
    constexpr unsigned NPOLY = 16;

    struct StereoFrame
    {
        std::array<float, NSTEREO> sample{};
    };


    struct VoiceContext
    {
        float pitch{};      // V/OCT relative to C4 (261.63 Hz).
        GateTriggerReceiver gateTriggerReceiver;

        void initialize()
        {
            pitch = 0;
            gateTriggerReceiver.initialize();
        }
    };


    struct VoiceEngine
    {
        virtual void initialize() = 0;
        virtual StereoFrame process(float sampleRateHz, const VoiceContext& context) = 0;
    };


    struct SineEngine : VoiceEngine
    {
        void initialize() override;
        StereoFrame process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct PolyStereoFrame
    {
        unsigned nchannels{};
        std::array<StereoFrame, NPOLY> poly{};
    };


    struct PolyVoiceEngineBase
    {
        std::array<VoiceContext, NPOLY> contextArray;

        virtual void initialize()
        {
            for (VoiceContext& context : contextArray)
                context.initialize();
        }

        virtual PolyStereoFrame process(float sampleRateHz, unsigned nchannels) = 0;
    };


    template <typename engine_t>
    struct PolyVoiceEngine : PolyVoiceEngineBase
    {
        std::array<engine_t, NPOLY> engineArray;

        void initialize() override
        {
            PolyVoiceEngineBase::initialize();
            for (engine_t& engine : engineArray)
                engine.initialize();
        }

        PolyStereoFrame process(float sampleRateHz, unsigned nchannels) override
        {
            PolyStereoFrame poly;
            poly.nchannels = std::clamp<unsigned>(nchannels, 0, NPOLY);
            for (unsigned c = 0; c < poly.nchannels; ++c)
                poly.poly[c] = engineArray[c].process(sampleRateHz, contextArray[c]);
            return poly;
        }
    };
}