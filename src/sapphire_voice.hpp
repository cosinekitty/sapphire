#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include "sapphire_gate_trigger.hpp"
#include "sapphire_engine.hpp"

namespace Sapphire
{
    constexpr unsigned NSTEREO = 2;
    constexpr unsigned NPOLY = 16;
    constexpr float VOICE_OCTAVE_SPAN = 4;       // +/- this many octaves from C4 center

    struct StereoFrame
    {
        std::array<float, NSTEREO> sample{};

        explicit StereoFrame() {}

        explicit StereoFrame(float left, float right)
        {
            sample[0] = left;
            sample[1] = right;
        }
    };


    struct VoiceContext
    {
        float pitch{};      // V/OCT relative to C4 (261.63 Hz).
        float freq{};       // pitch converted to Hz for your voice engine's convenience
        GateTriggerReceiver gateTriggerReceiver;

        explicit VoiceContext()
        {
            initialize();
        }

        void initialize()
        {
            setPitch(0);
            gateTriggerReceiver.initialize();
        }

        void setPitch(float voct)
        {
            if (!std::isfinite(voct))
                voct = 0;

            pitch = std::clamp<float>(voct, -VOICE_OCTAVE_SPAN, +VOICE_OCTAVE_SPAN);
            freq = std::exp2(pitch) * C4_FREQUENCY_HZ;
        }

        void setGateVoltage(float gateVoltage)
        {
            gateTriggerReceiver.update(gateVoltage);
        }
    };


    struct VoiceEngine
    {
        virtual void initialize() = 0;
        virtual StereoFrame process(float sampleRateHz, const VoiceContext& context) = 0;
    };


    struct SineEngine : VoiceEngine
    {
        float phase = 0;
        void initialize() override;
        StereoFrame process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct PolyStereoFrame
    {
        unsigned nchannels{};
        std::array<StereoFrame, NPOLY> poly;
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