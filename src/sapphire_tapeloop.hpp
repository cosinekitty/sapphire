#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "sapphire_engine.hpp"
#include "sapphire_crossfader.hpp"

namespace Sapphire
{
    constexpr float TAPELOOP_MIN_DELAY_SECONDS = 0.1;
    constexpr float TAPELOOP_MAX_DELAY_SECONDS = 10;
    constexpr float TAPELOOP_CROSSOVER_SECONDS = 0.05;
    static_assert(TAPELOOP_MAX_DELAY_SECONDS > TAPELOOP_MIN_DELAY_SECONDS);
    static_assert(TAPELOOP_CROSSOVER_SECONDS < TAPELOOP_MIN_DELAY_SECONDS);

    constexpr float TAPELOOP_RECORD_VOLTAGE_LIMIT = 100;
    constexpr unsigned TAPELOOP_MIN_SAMPLE_RATE_HZ = 1000;

    class TapeDelayMotor
    {
    private:
        float prevDelayTime{};
        LoHiPassFilter<float> filter;

    public:
        explicit TapeDelayMotor()
        {
            initialize();
        }

        void initialize()
        {
            setSlewRate(0.5);
            filter.Reset();
            prevDelayTime = -1;
        }

        float process(float requestedDelayTime, float sampleRateHz)
        {
            float targetDelayTime = std::clamp(requestedDelayTime, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
            if (prevDelayTime < 0)
            {
                prevDelayTime = targetDelayTime;
                filter.Snap(targetDelayTime);
                return targetDelayTime;
            }

            filter.Update(targetDelayTime, sampleRateHz);
            float rawDelayTime = filter.LoPass();

            constexpr float speedLimit = 0.9;
            float delayTimeChange = std::clamp(
                rawDelayTime - prevDelayTime,
                -speedLimit / sampleRateHz,
                +speedLimit / sampleRateHz
            );

            prevDelayTime = std::clamp(
                prevDelayTime + delayTimeChange,
                TAPELOOP_MIN_DELAY_SECONDS,
                TAPELOOP_MAX_DELAY_SECONDS
            );
            return prevDelayTime;
        }

        void setSlewRate(float knob)
        {
            constexpr float mix = 0.01;
            const float x = std::clamp<float>(knob, 0, 1) / 0.5;
            const float freq = 5 * Cube(mix + (1-mix)*x);
            filter.SetCutoffFrequency(freq);
        }
    };


    enum class InterpolatorKind
    {
        Linear,
        Sinc,
        LEN
    };


    class TapeLoop
    {
    private:
        static constexpr int windowSize = 3;
        using sinc_interpolator_t = Interpolator<float, windowSize>;

        float delayTimeSec = 0;
        float sampleRateHz = 0;
        double reversePlaybackHead = 0;
        int recordIndex = 0;
        std::vector<float> buffer;
        int bufferLength = 0;
        unsigned recoveryCountdown = 0;
        TapeDelayMotor tapeDelayMotor;
        InterpolatorKind ikind = InterpolatorKind::Linear;

        int wrapIndex(int position) const
        {
            const int length = static_cast<int>(buffer.size());
            return MOD(position, length);
        }

        const float& at(int position) const         // allowed to wrap around in +/- directions
        {
            return buffer[wrapIndex(position)];
        }

        float& at(int position)
        {
            return buffer[wrapIndex(position)];
        }

        float interpolate(float secondsIntoPast) const
        {
            float index = recordIndex - (secondsIntoPast * sampleRateHz);
            int position = static_cast<int>(std::round(index));
            float offset = index - position;
            assert(std::abs(offset) <= 0.501);

            switch (ikind)
            {
                case InterpolatorKind::Linear:
                default:
                {
                    float yc = at(position);    // center signal
                    float yn;   // next signal, in direction of `offset`
                    if (offset >= 0)
                    {
                        yn = at(position+1);
                    }
                    else
                    {
                        yn = at(position-1);
                        offset = -offset;       // toggle sign to force positive
                    }
                    return (1-offset)*yc + offset*yn;
                }

                case InterpolatorKind::Sinc:
                {
                    sinc_interpolator_t interp;
                    for (int w = -windowSize; w <= +windowSize; ++w)
                        interp.write(w, at(position + w));

                    return interp.read(offset);
                }
            }
        }

    public:
        explicit TapeLoop()
        {
            initialize();
        }

        void initialize()
        {
            recordIndex = 0;
            reversePlaybackHead = 0;
            recoveryCountdown = 0;
            tapeDelayMotor.initialize();
            clear();
        }

        void clear()
        {
            for (float& x : buffer)
                x = 0;
        }

        bool isRecoveringFromOverload() const
        {
            return recoveryCountdown > 0;
        }

        static bool IsValidSampleRate(float sr)
        {
            return std::isfinite(sr) && (sr >= TAPELOOP_MIN_SAMPLE_RATE_HZ);
        }

        void setInterpolatorKind(InterpolatorKind kind)
        {
            ikind = kind;
        }

        void setSlewRate(float _slewRate)
        {
            tapeDelayMotor.setSlewRate(_slewRate);
        }

        bool setDelayTime(float _delayTimeSec, float _sampleRateHz)
        {
            if (!std::isfinite(_delayTimeSec))
                return false;

            if (!IsValidSampleRate(_sampleRateHz))
                return false;

            if (sampleRateHz != _sampleRateHz)
            {
                // The first time we know the sample rate, or any time it changes,
                // resize the buffer to allow the maximum possible number of samples
                // as required by the new sample rate.

                sampleRateHz = _sampleRateHz;

                const float maxTime = TAPELOOP_MAX_DELAY_SECONDS + TAPELOOP_CROSSOVER_SECONDS;
                const int maxSamples = static_cast<int>(std::ceil(_sampleRateHz * maxTime));
                if (maxSamples <= 0)
                    return false;

                bufferLength = maxSamples;
                buffer.resize(bufferLength);
                clear();    // any audio in the buffer already is recorded at the wrong sample rate
            }

            delayTimeSec = tapeDelayMotor.process(_delayTimeSec, _sampleRateHz);
            return true;
        }

        float recall(float secondsIntoPast) const
        {
            const float offsetSeconds = FMOD(secondsIntoPast, delayTimeSec);
            float newer = interpolate(offsetSeconds);
            if (offsetSeconds >= TAPELOOP_CROSSOVER_SECONDS)
                return newer;

            // We are slightly in the past, close behind the record head.
            // Use crossover fading between newly recorded data and older data.
            float older = interpolate(offsetSeconds + delayTimeSec);
            float mix = offsetSeconds / TAPELOOP_CROSSOVER_SECONDS;
            return mix*newer + (1-mix)*older;
        }

        float readForward() const
        {
            return recall(0);
        }


        void updateReversePlaybackHead()
        {
            const double incr = 2.0 / static_cast<double>(sampleRateHz);
            reversePlaybackHead = FMOD<double>(reversePlaybackHead + incr, delayTimeSec);
        }


        float readReverse() const
        {
            return recall(reversePlaybackHead);
        }


        bool write(float sample, float clearSmootherGain)
        {
            // Protect the tape loop from NAN/infinite/crazy voltages.
            float safe = 0;
            if (recoveryCountdown > 0)
            {
                --recoveryCountdown;
            }
            else if (std::isfinite(sample) && std::abs(sample) <= TAPELOOP_RECORD_VOLTAGE_LIMIT)
            {
                safe = sample;
            }
            else
            {
                clear();
                if (IsValidSampleRate(sampleRateHz))
                    recoveryCountdown = static_cast<unsigned>(sampleRateHz);
                else
                    recoveryCountdown = 48000;
            }

            buffer[recordIndex] = safe * clearSmootherGain;
            recordIndex = (recordIndex + 1) % bufferLength;
            return recoveryCountdown == 0;
        }
    };
}
