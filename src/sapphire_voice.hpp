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


    inline float MapParameter(float value, float oldMin, float oldMax, float newMin, float newMax)
    {
        const float u = std::clamp<float>((value - oldMin) / (oldMax - oldMin), 0, 1);
        return newMin + u*(newMax - newMin);
    }


    inline float MapKnob(float knob, float newMin, float newMax)
    {
        return MapParameter(knob, -1, +1, newMin, newMax);
    }


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


    constexpr unsigned NUM_DYNAMIC_PARAMS = 4;


    struct VoiceContext
    {
        float pitch{};      // V/OCT relative to C4 (261.63 Hz).
        float freq{};       // pitch converted to Hz for your voice engine's convenience
        GateTriggerReceiver gateTriggerReceiver;
        float attack{};
        float decay{};
        float sustain{};
        float release{};
        float mod[NUM_DYNAMIC_PARAMS]{};     // Dynamic parameters. Their meaning depends on the selected engine.

        explicit VoiceContext()
        {
            initialize();
        }

        void initialize()
        {
            gateTriggerReceiver.initialize();
        }

        void setPitch(float voct)
        {
            pitch = HealNumber<float>(voct, 0);
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
            return TenToPower(2*knob - 1);
        }

        double process(float sampleRateHz, const VoiceContext &context);
    };


    using blep_t = rack::dsp::MinBlepGenerator<16, 16, float>;


    struct MonoSideInfo         // any parameters that differ from left/right sides
    {
        float detuneFactor{};
    };


    inline float DeltaPhase(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side)
    {
        // Return change in 0 <= phase <= 1 over expressed in fractional periods/sample.
        return (context.freq * side.detuneFactor) / sampleRateHz;
    }


    struct MonoVoiceEngine
    {
        float phase = 0;            // 0 <= phase < 1
        float square = 0;           // pure signal at ±1, not scaled for 5 volts.
        bool prevState = false;
        blep_t blep;                // for anti-aliasing discontinuities between samples
        bool pwmOverrideFiftyPercent = false;  // triangle hack: always have 50% duty cycle for underlying square wave

        virtual void initialize() = 0;
        virtual float process(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side) = 0;
        float blepSquare(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side);
    };


    struct SineEngine : MonoVoiceEngine
    {
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct TriangleEngine : MonoVoiceEngine
    {
        float triangle = 0;    // integral of BLEP-ed square

        TriangleEngine()
        {
            pwmOverrideFiftyPercent = true;
        }

        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct SawEngine : MonoVoiceEngine
    {
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct SquareEngine : MonoVoiceEngine
    {
        void initialize() override;
        float process(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side) override;
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
                // Calculate detune.
                float dial = Cube<float>(context.mod[0] + 1) / 8;

                MonoSideInfo leftSide;
                leftSide.detuneFactor = 1 + dial*0.007;

                MonoSideInfo rightSide;
                rightSide.detuneFactor = 1 / leftSide.detuneFactor;

                const float env = envelope.process(sampleRateHz, context);
                const float L = left .process(sampleRateHz, context, leftSide);
                const float R = right.process(sampleRateHz, context, rightSide);
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