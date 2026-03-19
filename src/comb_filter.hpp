#pragma once
#include <cmath>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    template <typename value_t>
    class CombFilter
    {
    private:
        static constexpr float maxFeedback = 0.999;
        static constexpr int windowSize = 2;
        using interpolator_t = Interpolator<value_t, windowSize>;
        using delay_t = DelayLine<value_t, 10000>;
        delay_t delay;
        float feedback{};

    public:
        void initialize()
        {
            delay.clear();
        }

        void setFeedback(float knob)
        {
            feedback = maxFeedback * std::clamp<float>(knob, -1, +1);
        }

        value_t process(float sampleRateHz, float delayTimeSeconds, value_t inSample)
        {
            delay.write(inSample);

            float delaySamples = sampleRateHz * delayTimeSeconds;
            std::size_t centerSample = static_cast<std::size_t>(std::round(delaySamples));

            interpolator_t interp;
            for (int w = -windowSize; w <= +windowSize; ++w)
            {
                value_t pastSample = delay.readBackward(centerSample + w);
                interp.write(w, pastSample);
            }
            value_t oldSample = interp.read(delaySamples - centerSample);

            return inSample + feedback*oldSample;
        }
    };
}
