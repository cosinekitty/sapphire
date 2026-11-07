#pragma once
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include "rack.hpp"
#include "sapphire_gate_trigger.hpp"
#include "sapphire_engine.hpp"
#include "sapphire_random.hpp"

namespace Sapphire
{
    constexpr unsigned NSTEREO = 2;
    constexpr unsigned NPOLY = 16;
    constexpr float VOICE_OCTAVE_SPAN = 4.5;       // +/- this many octaves from C4 center (match VCV VCO range)
    constexpr float VOICE_OCTAVE_FADE = 0.5;       // what portion of the upper VOICE_OCTAVE_SPAN fades amplitude to zero
    constexpr float PEAK_VOLTS = 5;

    constexpr float MapKnob(float x, float y1, float y2)
    {
        return Remap<float>(x, -1, +1, y1, y2);
    }

    constexpr float AmplitudeForPitch(float voct)
    {
        if (voct <= -VOICE_OCTAVE_SPAN || voct >= +VOICE_OCTAVE_SPAN)
            return 0;

        if (voct <= -VOICE_OCTAVE_SPAN + VOICE_OCTAVE_FADE)
            return (voct + VOICE_OCTAVE_SPAN) / VOICE_OCTAVE_FADE;

        if (voct >= VOICE_OCTAVE_SPAN - VOICE_OCTAVE_FADE)
            return (VOICE_OCTAVE_SPAN - voct) / VOICE_OCTAVE_FADE;

        return 1;
    }

    struct StereoFrame
    {
        std::array<float, NSTEREO> sample{};
        float phase{};

        explicit StereoFrame() {}

        explicit StereoFrame(float left, float right, float _phase)
        {
            sample[0] = left;
            sample[1] = right;
            phase = _phase;
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
        SapphireRandom rand{rack::random::u64()};

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

        float amplitudeForPitch() const
        {
            return AmplitudeForPitch(getPitchVoct());
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

        static double RampTimeSeconds(double knob)
        {
            return TenToPower(2*knob - 1);
        }

        double process(float sampleRateHz, VoiceContext &context);
    };


    using blep_t = rack::dsp::MinBlepGenerator<16, 16, float>;


    struct MonoSideInfo         // any parameters that differ from left/right sides
    {
        float detuneFactor{};
    };


    inline float InstantFrequency(const VoiceContext& context, const MonoSideInfo& side)
    {
        return context.getFrequency() * side.detuneFactor;
    }


    inline float DeltaPhase(float sampleRateHz, const VoiceContext& context, const MonoSideInfo& side)
    {
        // Return change in 0 <= phase <= 1 over expressed in fractional periods/sample.
        return InstantFrequency(context, side) / sampleRateHz;
    }


    constexpr unsigned VoiceOversample = 4;
    constexpr unsigned VoiceQuality = 32;
    constexpr float VoiceDecimatorCutoff = 0.17;    // mitigate aliasing while preserving high frequency response
    using decimator_t = rack::dsp::Decimator<VoiceOversample, VoiceQuality, float>;


    struct BlepSquareResult
    {
        float delta{};
        float area{};

        explicit BlepSquareResult(float _delta, float _area)
            : delta(_delta)
            , area(_area)
            {}
    };


    struct MonoVoiceEngine
    {
        float phase = 0;            // 0 <= phase < 1
        float square = 0;           // pure signal at ±1, not scaled for 5 volts.
        bool prevState = false;     // helps track phase wraparound from 1 back to 0.
        blep_t blep;                // for anti-aliasing discontinuities between samples
        bool supportsSaturation = false;
        bool usesOversampling = false;
        decimator_t decimator{VoiceDecimatorCutoff};

        virtual void initialize(SapphireRandom& rand) = 0;
        virtual float update(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side) = 0;
        BlepSquareResult blepSquare(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side);

        float process(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side);
    };


    struct SineEngine : MonoVoiceEngine
    {
        float y1{};
        float y2{};

        explicit SineEngine()
        {
            supportsSaturation = true;
            usesOversampling = true;
        }
        void initialize(SapphireRandom& rand) override;
        float update(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct TriangleEngine : MonoVoiceEngine
    {
        float triangle = 0;    // integral of BLEP-ed square

        void initialize(SapphireRandom& rand) override;
        float update(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct SawEngine : MonoVoiceEngine
    {
        void initialize(SapphireRandom& rand) override;
        float update(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct SquareEngine : MonoVoiceEngine
    {
        void initialize(SapphireRandom& rand) override;
        float update(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct WindEngine : MonoVoiceEngine
    {
        CascadeStateVariableFilter<float,3> filter;
        std::normal_distribution<float> dist {0.0f, 1.0f};
        float resknob = -1;

        explicit WindEngine()
        {
            supportsSaturation = true;
            filter.initialize();
            filter.mask = NEED_BP;
            filter.setCascade(3);
        }

        void initialize(SapphireRandom& rand) override;
        float update(float sampleRateHz, VoiceContext& context, const MonoSideInfo& side) override;
    };


    struct PolyStereoFrame
    {
        unsigned nchannels{};
        std::array<StereoFrame, NPOLY> poly;
    };


    struct ScalarFrame
    {
        unsigned nchannels{};
        std::array<float, NPOLY> poly{};
    };


    struct PolyResult
    {
        PolyStereoFrame stereo;
        ScalarFrame env;
        ScalarFrame phase;

        void setChannelCount(unsigned n)
        {
            assert(n <= NPOLY);
            stereo.nchannels = n;
            env.nchannels = n;
            phase.nchannels = n;
        }
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
        {
            PolyStereoVoice_initialize();
        }

        void PolyStereoVoice_initialize()
        {
            for (VoiceContext& context : contextArray)
                context.initialize();
        }

        virtual void initialize()
        {
            PolyStereoVoice_initialize();
        }

        virtual PolyResult process(float sampleRateHz, unsigned nchannels) = 0;
    };


    template <typename mono_engine_t, unsigned MonoVoiceLog = 1>
    struct AdsrVoiceEngine : PolyStereoVoice
    {
        const uint64_t seed{rack::random::u64()};
        static_assert(MonoVoiceLog >= 1);   // min = 2 voices (required for stereo)
        static_assert(MonoVoiceLog <= 8);   // max = 256 voices (absolute limit for CPU usage)
        static constexpr unsigned MonoVoiceCount = (1 << MonoVoiceLog);

        using lowpass_filter_t = StateVariableFilter<float>;

        struct stereo_source_t
        {
            std::array<mono_engine_t, MonoVoiceCount> chorus;
            std::array<lowpass_filter_t, MonoVoiceCount> lopass{};  // lazy-initialized via flag `needSnap`
            AdsrEnvelope  envelope;
            bool needSnap = true;
            float lpResonance = 0.3;
            float detuneWeight = 1;

            void initialize(SapphireRandom& rand)
            {
                for (mono_engine_t& voice : this->chorus)
                    voice.initialize(rand);

                envelope.initialize();
                needSnap = true;        // initialize lopass filters right before processing the next audio frame
            }

            float saturation(float v, float d)
            {
                static constexpr float margin = 0.001;
                static constexpr float headroom = PEAK_VOLTS * 1.05;

                const float dabs = std::abs(d);
                if (!chorus[0].supportsSaturation || (dabs <= margin))
                    return v;

                const float x = v / headroom;
                const float k = 1 + 5*dabs;
                float y;
                if (d > +margin)
                {
                    y = std::tanh(k*x) / std::tanh(k);
                }
                else
                {
                    static constexpr float R = 0.36;
                    static constexpr float Q = 2.5;
                    y = std::sin(R*(k + Q)*x);
                }

                y *= PEAK_VOLTS;

                static constexpr float fade = 0.1;
                const float u = (dabs - margin) / fade;
                if (u >= 0 && u <= 1)
                    y = LinearMix<float>(u, v, y);

                return y;
            }

            float updateVoice(unsigned k, float sampleRateHz, VoiceContext& context, MonoSideInfo& side, float env, float lopassCornerFreqHz)
            {
                float audio = chorus[k].process(sampleRateHz, context, side);
                audio = saturation(audio, context.mod[1]);
                audio = lopass[k].process(sampleRateHz, lopassCornerFreqHz, lpResonance, env*audio).lowpass;
                return audio;
            }

            StereoFrame process(float sampleRateHz, VoiceContext& context)
            {
                float env = envelope.process(sampleRateHz, context);

                // In addition to the ADSR envelope, we have a frequency attenuation envelope.
                // Always track the center frequency pitch.
                env *= context.amplitudeForPitch();

                if (env == 0)   // Are we completely silent right now?
                {
                    // Anti-click: force lazy init of the lowpass filters as soon as we are no longer silent.
                    needSnap = true;

                    // Don't burn CPU updating the voice models on silent samples.
                    return StereoFrame(0, 0, 0);
                }

                // Calculate detune.
                const float detune = context.mod[0];    // knob range = [-1, +1].

                std::array<MonoSideInfo, MonoVoiceCount> sideArray;
                static_assert(MonoVoiceCount >= 2);     // otherwise we can't have stereo detuning

                // Calculate a spectrum of detuning factors to smear out the pitch frequency.
                const float tuneShift = 0.014 * detuneWeight * FourthPower(detune);
                float loFactor = 1 + tuneShift;
                float hiFactor = 1 - tuneShift;
                if (detune < 0)
                    std::swap(loFactor, hiFactor);

                for (unsigned k = 0; k < MonoVoiceCount; ++k)
                {
                    float u = static_cast<float>(k) / (MonoVoiceCount-1);
                    sideArray[k].detuneFactor = LinearMix<float>(u, loFactor, hiFactor);
                }

                const float lopassCornerFreqHz = std::min<float>(
                    std::exp2(1.5 + 5.5*context.mod[3]) * C4_FREQUENCY_HZ,
                    sampleRateHz/2
                );

                if (needSnap)
                {
                    needSnap = false;
                    for (auto& f : lopass)
                        f.initialize();
                }

                if constexpr (MonoVoiceCount == 2)
                {
                    // Most stereo models are simply a pair of two voices: (left, right).
                    const float L = updateVoice(0, sampleRateHz, context, sideArray[0], env, lopassCornerFreqHz);
                    const float R = updateVoice(1, sampleRateHz, context, sideArray[1], env, lopassCornerFreqHz);
                    const PanningFactors pf = Panning(context.pan);
                    return StereoFrame(pf.left*L, pf.right*R, chorus[0].phase);
                }
                else
                {
                    // Models like 'saw' use a chorus effect with more than 2 voices running.
                    // Use the PAN knob's raw range [-1, +1] for all intermediate angular calculations.
                    // This allows us to do all the angle calculations first, then call Panning().
                    // Center of monaural field is at 0 "knobs", my new unit of angular measure.
                    // The left ear is at -1 knobs, the right ear is at +1 knobs.
                    // Arrange the voices around a semicircular arc in the stereo knob range [-1, +1].

                    // CHORUS: the number of saw engines we actually render (variable CPU load).
                    const float chorusCountRaw = std::exp2(MapKnob(context.mod[1], 1, MonoVoiceLog));
                    const unsigned chorusCount = std::clamp<unsigned>(std::ceil(chorusCountRaw), 2, MonoVoiceCount);

                    // The center angle is defined by context.pan.
                    // The total angular spread is defined by the SPREAD control.
                    static constexpr float maxSpread = 10.0;
                    const float spread = maxSpread * Cube(MapKnob(context.mod[2], 0, 1));
                    float leftSum = 0;
                    float rightSum = 0;
                    for (unsigned k = 0; k < chorusCount; ++k)
                    {
                        float audio = updateVoice(k, sampleRateHz, context, sideArray[k], env, lopassCornerFreqHz);
                        if (k+1 == chorusCount)
                            audio *= (1 - (chorusCount-chorusCountRaw));   // final voice fader/attenuation
                        const float pos = Remap<float>(k, 0, chorusCountRaw, context.pan-spread, context.pan+spread);
                        const PanningFactors pf = Panning(pos);
                        leftSum  += pf.left  * audio;
                        rightSum += pf.right * audio;
                    }

                    return StereoFrame(
                        leftSum  / chorusCountRaw,
                        rightSum / chorusCountRaw,
                        chorus[0].phase
                    );
                }
            }
        };

        std::array<stereo_source_t, NPOLY> stereoSourceArray;

        explicit AdsrVoiceEngine(const char *_name, const char *m0, const char *m1, const char *m2, const char *m3, float detuneWeight = 1.0)
            : PolyStereoVoice(_name, m0, m1, m2, m3)
        {
            for (stereo_source_t& source : stereoSourceArray)
                source.detuneWeight = detuneWeight;

            AdsrVoiceEngine_initialize();
        }

        void AdsrVoiceEngine_initialize()
        {
            SapphireRandom rand{seed};
            for (stereo_source_t& source : stereoSourceArray)
                source.initialize(rand);
        }

        void initialize() override
        {
            PolyStereoVoice::initialize();
            AdsrVoiceEngine_initialize();
        }

        PolyResult process(float sampleRateHz, unsigned nchannels) override
        {
            PolyResult result;
            result.setChannelCount(nchannels);
            for (unsigned c = 0; c < nchannels; ++c)
            {
                stereo_source_t& source = stereoSourceArray[c];
                result.stereo.poly[c] = source.process(sampleRateHz, contextArray[c]);
                result.phase.poly[c] = source.chorus[0].phase;
                result.env.poly[c] = source.envelope.fraction;
            }
            return result;
        }
    };
}