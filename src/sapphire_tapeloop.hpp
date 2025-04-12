#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    constexpr float TAPELOOP_MIN_DELAY_SECONDS = 0.001;
    constexpr float TAPELOOP_MAX_DELAY_SECONDS = 10;
    constexpr float TAPELOOP_DEFAULT_DELAY_SECONDS = 1;
    constexpr float TAPELOOP_RECORD_VOLTAGE_LIMIT = 100;
    constexpr unsigned TAPELOOP_MIN_SAMPLE_RATE_HZ = 1000;

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
        }

        void clear()
        {
            for (float& x : buffer)
                x = 0;
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

            if (sampleRateHz != _sampleRateHz)
            {
                sampleRateHz = _sampleRateHz;

                // The first time we know the sample rate, or any time it changes,
                // resize the buffer to allow the maximum possible number of samples
                // as required by the new sample rate.
                const int cushion = 16;     // gives space to interpolate around the boundary and avoid weird cases

                // Because the tape loop is reversible, we need twice as much tape time as the maximum delay time.

                const int maxSizeForSampleRate = cushion + static_cast<int>(std::ceil(2 * sampleRateHz * TAPELOOP_MAX_DELAY_SECONDS));
                buffer.resize(maxSizeForSampleRate);
                clear();
            }

            delayTimeSec = std::clamp(_delayTimeSec, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
            totalLoopSamples = static_cast<int>(std::round(delayTimeSec * sampleRateHz));
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

        void showRecorderError()
        {
            // FIXFIXFIX - light up some kind of error on the display
        }

        void hideRecorderError()
        {
            // FIXFIXFIX - clear the display of the error indicator, if it is active.
        }

        void write(float sample, float sampleRateHz)
        {
            // Protect the tape loop from NAN/infinite/crazy voltages.
            float safe = 0;
            if (recoveryCountdown > 0)
            {
                --recoveryCountdown;
            }
            else if (std::isfinite(sample) && std::abs(sample) <= TAPELOOP_RECORD_VOLTAGE_LIMIT)
            {
                hideRecorderError();
                safe = sample;
            }
            else
            {
                showRecorderError();
                if (IsValidSampleRate(sampleRateHz))
                    recoveryCountdown = static_cast<unsigned>(sampleRateHz / 4);
                else
                    recoveryCountdown = TAPELOOP_MIN_SAMPLE_RATE_HZ / 4;
            }

            buffer.at(recordIndex) = safe;
            recordIndex = wrapIndex(recordIndex + getTapeDirection());
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
