#pragma once
#include <cmath>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    template <typename value_t>
    class CombFilter
    {
    private:
        static constexpr float CLAMP_LIMIT_VOLTS = 6.5;
        static constexpr float FEEDBACK_LIMIT = 0.999;
        static constexpr int windowSize = 2;

        using interpolator_t = Interpolator<value_t, windowSize>;
        using delay_t = DelayLine<value_t, 10000>;
        using filter_t = LoHiPassFilter<value_t>;

        delay_t delay;
        float resonance{};
        float frequencyVoct{};
        filter_t dcRejectFilter;

        static value_t Compress(value_t x)
        {
            return CLAMP_LIMIT_VOLTS * std::tanh(x / CLAMP_LIMIT_VOLTS);
        }

    public:
        void initialize()
        {
            delay.clear();

            dcRejectFilter.Reset();
            dcRejectFilter.SetCutoffFrequency(10.0);
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
            dcRejectFilter.Update(inSample, sampleRateHz);
            value_t filtSample = dcRejectFilter.HiPass();

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
            value_t feedbackSample = Compress(filtSample + resonance*oldSample);
            delay.write(feedbackSample);
            return feedbackSample;
        }
    };
}
