#pragma once
#include <cmath>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    template <typename value_t>
    class CombFilter
    {
    private:
        static constexpr float FEEDBACK_LIMIT = 0.999;
        static constexpr int windowSize = 2;
        using interpolator_t = Interpolator<value_t, windowSize>;
        using delay_t = DelayLine<value_t, 10000>;
        delay_t delay;
        float resonance{};
        float frequencyVoct{};

    public:
        void initialize()
        {
            delay.clear();
        }

        void setResonance(float knob)
        {
            resonance = FEEDBACK_LIMIT * std::clamp<float>(knob, -1, +1);
        }

        void setFrequency(float knob)
        {
            frequencyVoct = std::clamp<float>(knob, -Gravy::OctaveRange, +Gravy::OctaveRange);
        }

        value_t process(float sampleRateHz, value_t inSample)
        {
            delay.write(inSample);

            float frequencyHz = TwoToPower(frequencyVoct) * Gravy::DefaultFrequencyHz;
            float delaySamples = sampleRateHz / frequencyHz;
            std::size_t centerSample = static_cast<std::size_t>(std::round(delaySamples));

            interpolator_t interp;
            for (int w = -windowSize; w <= +windowSize; ++w)
            {
                value_t pastSample = delay.readBackward(centerSample + w);
                interp.write(w, pastSample);
            }
            value_t oldSample = interp.read(delaySamples - centerSample);

            return inSample + resonance*oldSample;
        }
    };
}
