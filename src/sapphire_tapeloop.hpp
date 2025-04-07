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
        float playbackIndex = 0;        // floating point index means we use sinc-interpolator for reads
        int recordIndex = 0;
        int tapeDirection = +1;         // +1 = forward, -1 = reverse
        std::vector<float> buffer;

        const float& at(int position) const         // allowed to wrap around in +/- directions
        {
            const int length = static_cast<int>(buffer.size());
            const int index = MOD(position, length);
            return buffer.at(index);
        }

        float& at(int position)
        {
            return const_cast<float&>(const_cast<const TapeLoop*>(this)->at(position));
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

        float read(float secondsIntoPast)
        {
            float tSample = recordIndex - secondsIntoPast*sampleRateHz;

            // FIXFIXFIX use interpolator - for now just snap to nearest integer index
            int position = static_cast<int>(std::round(tSample));
            return at(position);
        }

        float read()
        {
            return read(delayTimeSec);
        }

        void write(float sample)
        {
            buffer.at(recordIndex) = sample;
            recordIndex += tapeDirection;
        }

        bool isReversed() const
        {
            return tapeDirection < 0;
        }

        void setReversed(bool reverse)
        {
            tapeDirection = reverse ? -1 : +1;
        }
    };
}
