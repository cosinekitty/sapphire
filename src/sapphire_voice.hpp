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


    enum class PitchMode
    {
        Follow,
        SampleHold,
        Glide,
    };


    struct PitchTracker
    {
        PitchMode mode{};
        float pitch{};       // V/OCT relative to C4 (261.63 Hz).
        unsigned counter = 0;   // remaining frames before latch occurs
        unsigned limit = 48;    // roughly 1 ms at 48 kHz, but caller is free to calculate more precisely
        bool latched = false;

        void initialize()
        {
            latched = false;
            counter = 0;
        }

        void update(const GateTriggerReceiver& receiver, float voct, float glideOctavesPerSample)
        {
            switch (mode)
            {
                case PitchMode::Follow:
                default:
                {
                    pitch = voct;
                    latched = false;
                }
                break;

                case PitchMode::SampleHold:
                {
                    if (receiver.isTriggerActive())
                    {
                        latched = false;
                        counter = 0;
                    }
                    else if (!latched && receiver.isGateActive())
                    {
                        if (counter < limit)
                            ++counter;
                        else
                            latched = true;
                    }

                    if (!latched)
                        pitch = voct;
                }
                break;

                case PitchMode::Glide:
                {
                    latched = false;
                    if (pitch < voct)
                        pitch = std::min<float>(voct, pitch + glideOctavesPerSample);
                    else if (pitch > voct)
                        pitch = std::max<float>(voct, pitch - glideOctavesPerSample);
                }
                break;
            }
        }
    };


    struct VoiceContext
    {
        GateTriggerReceiver gateTriggerReceiver;
        float attack{};
        float decay{};
        float sustain{};
        float release{};
        float mod[NUM_DYNAMIC_PARAMS]{};     // Dynamic parameters. Their meaning depends on the selected engine.
        float pan{};        // -1 .. +1
        PitchTracker trackerPitch;
        PitchTracker trackerFreq;
        PitchTracker trackerOct;

        explicit VoiceContext()
        {
            initialize();
        }

        void initialize()
        {
            gateTriggerReceiver.initialize();
            trackerPitch.initialize();
            trackerFreq.initialize();
            trackerOct.initialize();
        }

        void updateGatePitch(float gateVoltage, float pitch, float freq, float oct, float glideOctavesPerSample)
        {
            gateTriggerReceiver.update(gateVoltage);
            trackerPitch.update(gateTriggerReceiver, pitch, glideOctavesPerSample);
            trackerFreq.update(gateTriggerReceiver, freq, glideOctavesPerSample);
            trackerOct.update(gateTriggerReceiver, oct, glideOctavesPerSample);
        }

        float getPitchVoct() const
        {
            return trackerPitch.pitch + trackerFreq.pitch + trackerOct.pitch;
        }

        float getFrequency() const
        {
            return std::exp2(getPitchVoct()) * C4_FREQUENCY_HZ;
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
        return (context.getFrequency() * side.detuneFactor) / sampleRateHz;
    }


    struct MonoVoiceEngine
    {
        float phase = 0;            // 0 <= phase < 1
        float square = 0;           // pure signal at ±1, not scaled for 5 volts.
        bool prevState = false;
        blep_t blep;                // for anti-aliasing discontinuities between samples
        bool pwmOverrideFiftyPercent = false;  // triangle hack: always have 50% duty cycle for underlying square wave
        bool supportsDistortion = false;

        virtual void initialize() = 0;
        virtual float process(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side) = 0;
        float blepSquare(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side);
    };


    struct SineEngine : MonoVoiceEngine
    {
        explicit SineEngine()
        {
            supportsDistortion = true;
        }
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


    struct EnvelopeFrame
    {
        unsigned nchannels{};
        std::array<float, NPOLY> poly{};
    };


    struct PolyResult
    {
        PolyStereoFrame stereo;
        EnvelopeFrame env;
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

        virtual PolyResult process(float sampleRateHz, unsigned nchannels) = 0;
    };


    template <typename mono_engine_t>
    struct AdsrVoiceEngine : PolyStereoVoice
    {
        struct stereo_source_t
        {
            mono_engine_t left;
            mono_engine_t right;
            AdsrEnvelope  envelope;

            void initialize()
            {
                left.initialize();
                right.initialize();
                envelope.initialize();
            }

            float distortion(float v, float d)
            {
                static constexpr float margin = 0.001;
                static constexpr float fade = 0.1;
                static constexpr float headroom = PEAK_VOLTS * 1.05;
                float x = v / headroom;
                float y = x;
                if (left.supportsDistortion)
                {
                    const float dabs = std::abs(d);
                    const float dilate = 1 + 5*dabs;
                    if (d < -margin)
                        y = std::tanh(x*dilate);
                    else if (d > +margin)
                        y = BicubicLimiter<float>(x*dilate, 1);

                    const float u = (dabs - margin) / (fade - margin);
                    if (u >= 0 && u <= 1)
                        y = LinearMix<float>(u, x, y);
                }
                return y * headroom;
            }

            StereoFrame process(float sampleRateHz, const VoiceContext& context)
            {
                // Calculate detune.
                const float m = context.mod[0];
                const float a = 0.014 * FourthPower(m);

                MonoSideInfo leftSide, rightSide;

                if (m < 0)
                {
                    leftSide.detuneFactor  = 1-a;
                    rightSide.detuneFactor = 1+a;
                }
                else
                {
                    leftSide.detuneFactor  = 1+a;
                    rightSide.detuneFactor = 1-a;
                }

                const float env = envelope.process(sampleRateHz, context);
                if (env == 0)
                    return StereoFrame(0, 0);   // don't burn CPU updating the voice models on silent samples

                float L = env * left .process(sampleRateHz, context, leftSide);
                float R = env * right.process(sampleRateHz, context, rightSide);
                L = distortion(L, context.mod[1]);
                R = distortion(R, context.mod[1]);
                const PanningFactors pf = Panning(context.pan);
                return StereoFrame(pf.left*L, pf.right*R);
            }
        };

        std::array<stereo_source_t, NPOLY> stereoSourceArray;

        explicit AdsrVoiceEngine(const char *_name, const char *m0, const char *m1, const char *m2, const char *m3)
            : PolyStereoVoice(_name, m0, m1, m2, m3)
            {}

        void initialize() override
        {
            PolyStereoVoice::initialize();
            for (stereo_source_t& source : stereoSourceArray)
                source.initialize();
        }

        PolyResult process(float sampleRateHz, unsigned nchannels) override
        {
            PolyResult result;
            result.stereo.nchannels = result.env.nchannels = std::min<unsigned>(nchannels, NPOLY);
            for (unsigned c = 0; c < result.stereo.nchannels; ++c)
            {
                result.stereo.poly[c] = stereoSourceArray[c].process(sampleRateHz, contextArray[c]);
                result.env.poly[c] = stereoSourceArray[c].envelope.fraction;
            }
            return result;
        }
    };
}