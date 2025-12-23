#pragma once
#include "sapphire_engine.hpp"

namespace Sapphire
{
    namespace Chorus
    {
        constexpr float defaultChorusHz = 0.076;

        struct StereoFrame
        {
            float sample[2]{};

            StereoFrame() {}

            explicit StereoFrame(float left, float right)
                : sample{left, right}
                {}

            void operator += (const StereoFrame& other)
            {
                sample[0] += other.sample[0];
                sample[1] += other.sample[1];
            }

            friend StereoFrame operator* (float k, const StereoFrame& f)
            {
                return StereoFrame(k*f.sample[0], k*f.sample[1]);
            }

            friend StereoFrame operator+ (const StereoFrame& a, const StereoFrame& b)
            {
                return StereoFrame(a.sample[0] + b.sample[0], a.sample[1] + b.sample[1]);
            }
        };

        constexpr unsigned delayLineFrames = 48000;     // up to 1 second at typical rate, less at higher rates.
        using chorus_delay_t = DelayLine<StereoFrame, delayLineFrames>;

        constexpr float max_dt = 0.004;
        constexpr float inputScale = 1000;
        constexpr float outputScale = 1000 / inputScale;

        struct Engine
        {
            float chorusDepth{};
            float chorusRate{};
            float chorusAngle{};
            chorus_delay_t chorusDelay{};
            float gain{};
            unsigned resetSamples{};

            explicit Engine()
                {}

            void initialize()
            {
                setChorusDepth();
                setChorusRate();
                chorusAngle = 0;
                chorusDelay.clear();
                gain = 1;
                resetSamples = 0;
            }

            StereoFrame interpolateBackward(float sampleRateHz, float timeOffsetSec)
            {
                float s = std::max<float>(0, sampleRateHz * timeOffsetSec);
                unsigned s1 = static_cast<unsigned>(std::floor(s));
                float frac = s - s1;
                StereoFrame f1 = chorusDelay.readBackward(s1);
                StereoFrame f2 = chorusDelay.readBackward(s1+1);
                return (1-frac)*f1 + frac*f2;
            }

            StereoFrame update(float sampleRateHz, float left, float right)
            {
                if (resetSamples > 0)
                {
                    --resetSamples;
                    return StereoFrame(0, 0);
                }

                // Keep the chorusDelay consistently fed, whether or not chorus is enabled.
                // The CPU cost is very small, and it prevents unwanted glitches.
                chorusDelay.write(StereoFrame(left, right));
                if (chorusDepth > 0)
                {
                    // FIXFIXFIX: 3 phase-shifted copies of each voice, preserving (left, right) as a unit.
                    // Slowly modulate position in time using linear/sinc interpolation (like Echo).
                    constexpr float chorusMaxSeconds = 0.03;
                    constexpr float rateAdjust = 0.35;

                    // Calculate sample offsets into the past for the 3 chorus voices.
                    const float timeFactor = chorusDepth * (chorusMaxSeconds/2);
                    const float t0 = timeFactor*(1 - std::cos(chorusAngle));
                    const float t1 = timeFactor*(1 - std::cos(chorusAngle + (M_PI*2)/3));
                    const float t2 = timeFactor*(1 - std::cos(chorusAngle - (M_PI*2)/3));

                    StereoFrame f0 = interpolateBackward(sampleRateHz, t0);
                    StereoFrame f1 = interpolateBackward(sampleRateHz, t1);
                    StereoFrame f2 = interpolateBackward(sampleRateHz, t2);

                    // Mix with the original using chorusDepth as the mix parameter.
                    left  = (1-chorusDepth)*left  + (chorusDepth/3)*(f0.sample[0] + f1.sample[0] + f2.sample[0]);
                    right = (1-chorusDepth)*right + (chorusDepth/3)*(f0.sample[1] + f1.sample[1] + f2.sample[1]);

                    // Update chorusAngle for next iteration.
                    const float power = TenToPower<float>(rateAdjust * chorusRate);
                    const float increment = power * (2*M_PI)*(defaultChorusHz/sampleRateHz);
                    chorusAngle = std::fmod(chorusAngle + increment, 2*M_PI);
                }

                if (!std::isfinite(left) || !std::isfinite(right))
                {
                    initialize();
                    left = right = 0;
                    resetSamples = static_cast<unsigned>(sampleRateHz / 2);
                }

                return StereoFrame(left, right);
            }

            void setLevel(float knob = 1)
            {
                float k = std::clamp<float>(knob, 0, 2);
                gain = Cube(k);
            }

            void setChorusDepth(float knob = 0.5)
            {
                chorusDepth = std::clamp<float>(knob, 0, 1);
            }

            void setChorusRate(float knob = 0)
            {
                chorusRate = std::clamp<float>(knob, -1, +1);
            }
        };
    }
}
