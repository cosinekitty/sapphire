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
    constexpr float PEAK_VOLTS = 5;

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
        float attack{};
        float decay{};
        float sustain{};
        float release{};

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


    enum class AdsrState
    {
        Quiet,
        Attack,
        Decay,
        Release,
    };


    struct AdsrEnvelope
    {
        AdsrState state{};
        double fraction{};      // 0 = silence, 1 = full power envelope; linear scale

        void initialize()
        {
            state = AdsrState::Quiet;
            fraction = 0;
        }

        double rampTimeSeconds(double knob)
        {
            return std::pow(10.0, 2*knob - 1);
        }

        double process(float sampleRateHz, const VoiceContext &context)
        {
            const bool gate = context.gateTriggerReceiver.isGateActive();

            switch (state)
            {
                case AdsrState::Quiet:
                default:    // treat any invalid states as identical to Quiet
                {
                    // On the rising edge of a gate, begin the attack phase.
                    if (gate)
                    {
                        state = AdsrState::Attack;
                        fraction = 0;
                    }
                }
                break;

                case AdsrState::Attack:
                {
                    // Gradually rise from 0 to 1.
                    double rampSamples = sampleRateHz * rampTimeSeconds(context.attack);
                    fraction = std::clamp<double>(fraction + 1/rampSamples, 0, 1);

                    if (gate)
                    {
                        if (fraction == 1)
                            state = AdsrState::Decay;
                    }
                    else
                    {
                        state = AdsrState::Release;
                    }
                }
                break;

                case AdsrState::Decay:
                {
                    // Keep envelope at full power until the gate goes away.
                    // Later we will use the sustain control that fades to a DECAY percentage.
                    if (!gate)
                        state = AdsrState::Release;
                }
                break;

                case AdsrState::Release:
                {
                    double rampSamples = sampleRateHz * rampTimeSeconds(context.release);
                    fraction = std::clamp<double>(fraction - 1/rampSamples, 0, 1);
                    if (gate)
                    {
                        state = AdsrState::Attack;
                    }
                    else
                    {
                        if (fraction == 0)
                            state = AdsrState::Quiet;
                    }
                }
                break;
            }

            return fraction;
        }
    };


    struct VoiceEngine
    {
        AdsrEnvelope envelope;
        virtual void initialize() = 0;
        virtual StereoFrame process(float sampleRateHz, const VoiceContext& context) = 0;
        virtual std::string getName() const = 0;
    };


    struct SineEngine : VoiceEngine
    {
        std::string getName() const override { return "sine"; }
        float phase = 0;
        void initialize() override;
        StereoFrame process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct TriangleEngine : VoiceEngine
    {
        std::string getName() const override { return "triangle"; }
        float phase = 0;
        void initialize() override;
        StereoFrame process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct SawEngine : VoiceEngine
    {
        std::string getName() const override { return "saw"; }
        float phase = 0;
        void initialize() override;
        StereoFrame process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct SquareEngine : VoiceEngine
    {
        std::string getName() const override { return "square"; }
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

        virtual std::string getName() const = 0;
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

        std::string getName() const override
        {
            return engineArray.at(0).getName();
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