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


    struct MonoVoiceEngine
    {
        float phase = 0;            // 0 <= phase < 1
        float square = 0;           // pure signal at ±1, not scaled for 5 volts.
        bool prevState = false;
        blep_t blep;                // for anti-aliasing discontinuities between samples

        virtual void initialize() = 0;
        virtual float process(float sampleRateHz, const VoiceContext& context) = 0;
        void blepSquare(float sampleRateHz, const VoiceContext& context);
    };


    struct SineEngine : MonoVoiceEngine
    {
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct TriangleEngine : MonoVoiceEngine
    {
        float triangle = 0;    // integral of BLEP-ed square

        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct SawEngine : MonoVoiceEngine
    {
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct SquareEngine : MonoVoiceEngine
    {
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context) override;
    };


    struct PolyStereoFrame
    {
        unsigned nchannels{};
        std::array<StereoFrame, NPOLY> poly;
    };


    struct PolyStereoVoice
    {
        using string = std::string;

        string name;
        string mod0;
        string mod1;
        string mod2;
        string mod3;
        std::array<VoiceContext, NPOLY> contextArray;

        explicit PolyStereoVoice(const string& _name, const string& m0, const string& m1, const string& m2, const string& m3)
            : name(_name)
            , mod0(m0)
            , mod1(m1)
            , mod2(m2)
            , mod3(m3)
            {}

        virtual void initialize()
        {
            for (VoiceContext& context : contextArray)
                context.initialize();
        }

        virtual PolyStereoFrame process(float sampleRateHz, unsigned nchannels) = 0;
    };


    template <typename mono_engine_t>
    struct AdsrVoiceEngine : PolyStereoVoice
    {
        struct stereo_pair_t
        {
            mono_engine_t left;
            mono_engine_t right;
            AdsrEnvelope envelope;

            void initialize()
            {
                left.initialize();
                right.initialize();
                envelope.initialize();
            }

            StereoFrame process(float sampleRateHz, const VoiceContext& context)
            {
                const float env = envelope.process(sampleRateHz, context);
                const float L = left .process(sampleRateHz, context);
                const float R = right.process(sampleRateHz, context);
                return StereoFrame(env*L, env*R);
            }
        };

        std::array<stereo_pair_t, NPOLY> stereoPairArray;

        explicit AdsrVoiceEngine(const char *_name, const char *m0, const char *m1, const char *m2, const char *m3)
            : PolyStereoVoice(_name, m0, m1, m2, m3)
            {}

        void initialize() override
        {
            PolyStereoVoice::initialize();
            for (stereo_pair_t& pair : stereoPairArray)
                pair.initialize();
        }

        PolyStereoFrame process(float sampleRateHz, unsigned nchannels) override
        {
            PolyStereoFrame poly;
            poly.nchannels = std::clamp<unsigned>(nchannels, 0, NPOLY);
            for (unsigned c = 0; c < poly.nchannels; ++c)
                poly.poly[c] = stereoPairArray[c].process(sampleRateHz, contextArray[c]);
            return poly;
        }
    };
}