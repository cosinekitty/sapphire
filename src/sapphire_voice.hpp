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
    constexpr float VOICE_OCTAVE_SPAN = 4.5;       // +/- this many octaves from C4 center (match VCV VCO range)
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


    template <typename real_t>
    real_t HealNumber(real_t value, real_t fallback)
    {
        return std::isfinite(value) ? value : fallback;
    }


    struct VoiceContext
    {
        float pitch{};      // V/OCT relative to C4 (261.63 Hz).
        float freq{};       // pitch converted to Hz for your voice engine's convenience
        GateTriggerReceiver gateTriggerReceiver;
        float attack{};
        float decay{};
        float sustain{};
        float release{};
        float duty{};       // also known as "pulse width modulation", the fraction of time a square wave is high

        explicit VoiceContext()
        {
            initialize();
        }

        void initialize()
        {
            setPitch(0);
            gateTriggerReceiver.initialize();
            duty = 0.5;
        }

        void setPitch(float voct)
        {
            pitch = HealNumber<float>(voct, 0);
            freq = std::exp2(pitch) * C4_FREQUENCY_HZ;
        }

        void setDutyCycle(float pwm)
        {
            // Limit the duty cycle from 1% to 99%, so that the output cannot be silent.
            duty = HealNumber<float>(
                std::clamp<float>(pwm, 0.01, 0.99),
                0.5
            );
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
            return TenToPower(2*knob - 1);
        }

        double process(float sampleRateHz, const VoiceContext &context);
    };


    using blep_t = rack::dsp::MinBlepGenerator<16, 16, float>;


    struct VoiceEngine
    {
        float phase = 0;            // 0 <= phase < 1
        float square = 0;           // pure signal at ±1, not scaled for 5 volts.
        bool prevState = false;
        AdsrEnvelope envelope;
        blep_t blep;                // for anti-aliasing discontinuities between samples

        virtual void initialize() = 0;
        virtual float process(float sampleRateHz, const VoiceContext& context) = 0;
        virtual std::string getName() const = 0;
        void blepSquare(float sampleRateHz, const VoiceContext& context);
    };


    struct SineEngine : VoiceEngine
    {
        std::string getName() const override { return "sine"; }
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct TriangleEngine : VoiceEngine
    {
        float triangle = 0;    // integral of BLEP-ed square

        std::string getName() const override { return "triangle"; }
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct SawEngine : VoiceEngine
    {
        std::string getName() const override { return "saw"; }
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct SquareEngine : VoiceEngine
    {
        std::string getName() const override { return "square"; }
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
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
        struct stereo_pair_t
        {
            std::array<engine_t, NSTEREO> engine;

            void initialize()
            {
                engine[0].initialize();
                engine[1].initialize();
            }
        };

        std::array<stereo_pair_t, NPOLY> stereoPairArray;

        void initialize() override
        {
            PolyVoiceEngineBase::initialize();
            for (stereo_pair_t& pair : stereoPairArray)
                pair.initialize();
        }

        std::string getName() const override
        {
            return stereoPairArray[0].engine[0].getName();
        }

        PolyStereoFrame process(float sampleRateHz, unsigned nchannels) override
        {
            PolyStereoFrame poly;
            poly.nchannels = std::clamp<unsigned>(nchannels, 0, NPOLY);
            for (unsigned c = 0; c < poly.nchannels; ++c)
            {
                poly.poly[c].sample[0] = stereoPairArray[c].engine[0].process(sampleRateHz, contextArray[c]);
                poly.poly[c].sample[1] = stereoPairArray[c].engine[1].process(sampleRateHz, contextArray[c]);
            }
            return poly;
        }
    };
}