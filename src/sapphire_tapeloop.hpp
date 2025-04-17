#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    constexpr float TAPELOOP_MIN_DELAY_SECONDS = 0.1;
    constexpr float TAPELOOP_MAX_DELAY_SECONDS = 10;
    static_assert(TAPELOOP_MAX_DELAY_SECONDS > TAPELOOP_MIN_DELAY_SECONDS);

    constexpr float TAPELOOP_RECORD_VOLTAGE_LIMIT = 100;
    constexpr unsigned TAPELOOP_MIN_SAMPLE_RATE_HZ = 1000;


    class TapeDelayMotor
    {
    private:
        // Limit the "motor speed", which is just a mental model of how fast the delay
        // time (in seconds) is allowed to change (per second). Since we have seconds/second,
        // we end up with a dimensionless "speed" limit.
        float maxSpeed = 0.9;
        float prevDelayTime = -1;
        LoHiPassFilter<float> filter;

    public:
        void initialize()
        {
            filter.SetCutoffFrequency(5);
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
            float speed = std::clamp(sampleRateHz * (rawDelayTime - prevDelayTime), -maxSpeed, +maxSpeed);
            prevDelayTime = std::clamp(
                prevDelayTime + (speed / sampleRateHz),
                TAPELOOP_MIN_DELAY_SECONDS,
                TAPELOOP_MAX_DELAY_SECONDS
            );
            return prevDelayTime;
        }
    };


    class TapeLoop
    {
    private:
        float delayTimeSec = 0;
        float sampleRateHz = 0;
        int recordIndex = 0;
        bool reverseTape = false;
        std::vector<float> buffer;
        int totalLoopSamples = 0;           // (2*dt) expressed in samples, for wraparound logic
        unsigned recoveryCountdown = 0;
        TapeDelayMotor tapeDelayMotor;

        int getTapeDirection() const
        {
            return reverseTape ? -1 : +1;
        }

        int wrapIndex(int position) const
        {
            return MOD(position, totalLoopSamples);
        }

        const float& at(int position) const         // allowed to wrap around in +/- directions
        {
            const int index = wrapIndex(position);
            return buffer.at(index);
        }

        float& at(int position)
        {
            const int index = wrapIndex(position);
            return buffer.at(index);
        }

    public:
        explicit TapeLoop()
        {
        }

        void initialize()
        {
            recoveryCountdown = 0;
            tapeDelayMotor.initialize();
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

        bool setDelayTime(float _delayTimeSec, float _sampleRateHz)
        {
            if (!std::isfinite(_delayTimeSec))
                return false;

            if (!IsValidSampleRate(_sampleRateHz))
                return false;

            const int cushion = 16;     // gives space to interpolate around the boundary and avoid weird cases
            if (sampleRateHz != _sampleRateHz)
            {
                const int maxSamples = static_cast<int>(std::ceil(2 * _sampleRateHz * TAPELOOP_MAX_DELAY_SECONDS));
                if (maxSamples <= 0)
                    return false;

                const std::size_t maxSize = static_cast<std::size_t>(cushion + maxSamples);

                sampleRateHz = _sampleRateHz;

                // The first time we know the sample rate, or any time it changes,
                // resize the buffer to allow the maximum possible number of samples
                // as required by the new sample rate.

                // Because the tape loop is reversible, we need twice as much tape time as the maximum delay time.

                buffer.resize(maxSize);
                clear();    // any audio in the buffer already is recorded at the wrong sample rate
            }

            const int capacity = static_cast<int>(buffer.size());
            assert(capacity >= cushion);

            delayTimeSec = tapeDelayMotor.process(_delayTimeSec, _sampleRateHz);
            const int calculated = static_cast<int>(std::round(delayTimeSec * sampleRateHz));
            totalLoopSamples = std::clamp(calculated, cushion, capacity);
            return true;
        }

        float read(float secondsIntoPast) const
        {
            float index = recordIndex - (secondsIntoPast * sampleRateHz);

            // FIXFIXFIX use interpolator - for now just snap to nearest integer index
            int position = static_cast<int>(std::round(index));
            return at(position);
        }

        float read()
        {
            return read(delayTimeSec);
        }

        bool write(float sample, float sampleRateHz)
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

            buffer.at(recordIndex) = safe;
            recordIndex = wrapIndex(recordIndex + getTapeDirection());
            return recoveryCountdown == 0;
        }

        bool isReversed() const
        {
            return reverseTape;
        }

        void setReversed(bool reverse)
        {
            reverseTape = reverse;
        }
    };
}
