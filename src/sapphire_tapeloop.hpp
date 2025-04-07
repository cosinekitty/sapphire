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

    class TapeLoop
    {
    private:
        float delayTimeSec = 0;
        float sampleRateHz = 0;
        int recordIndex = 0;
        bool reverseTape = false;
        std::vector<float> buffer;

        int getBufferLength() const
        {
            return static_cast<int>(buffer.size());
        }

        int getTapeDirection() const
        {
            return reverseTape ? -1 : +1;
        }

        int wrapIndex(int position) const
        {
            return MOD(position, getBufferLength());
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

        void resize()
        {
            // Make the buffer big enough to handle the maximum recording time at this sample rate.
            const int cushion = 16;     // gives space to interpolate around the boundary and avoid weird cases
            const int maxSizeForSampleRate = cushion + static_cast<int>(std::ceil(sampleRateHz * TAPELOOP_MAX_DELAY_SECONDS));
            buffer.resize(maxSizeForSampleRate);
            clear();
        }

    public:
        explicit TapeLoop()
        {
        }

        void initialize()
        {
        }

        void clear()
        {
            for (float& x : buffer)
                x = 0;
        }

        bool setDelayTime(float _delayTimeSec, float _sampleRateHz)
        {
            if (!std::isfinite(_delayTimeSec))
                return false;

            if (!std::isfinite(_sampleRateHz) || _sampleRateHz < 1000)
                return false;

            if (sampleRateHz != _sampleRateHz)
            {
                sampleRateHz = _sampleRateHz;

                // The first time we know the sample rate, or any time it changes,
                // resize the buffer to allow the maximum possible number of samples
                // as required by the new sample rate.
                resize();
            }

            delayTimeSec = std::clamp(_delayTimeSec, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
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

        void write(float sample)
        {
            buffer.at(recordIndex) = sample;
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
