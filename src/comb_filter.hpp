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
        float centerFrequencyHz = C4_FREQUENCY_HZ;

        InterpolatorKind interpKind = InterpolatorKind::Default;

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
            frequencyVoct = knob;
        }

        value_t process(float sampleRateHz, value_t inSample)
        {
            dcRejectFilter.Update(inSample, sampleRateHz);
            value_t filtSample = dcRejectFilter.HiPass();

            float frequencyHz = TwoToPower(frequencyVoct) * centerFrequencyHz;
            float delaySamples = sampleRateHz / frequencyHz;

            value_t oldSample;
            if (interpKind == InterpolatorKind::Sinc)
            {
                // Slower sinc-resampler with (theoretically) better audio.
                std::size_t centerSample = static_cast<std::size_t>(std::round(delaySamples));
                interpolator_t interp;
                for (int w = -windowSize; w <= +windowSize; ++w)
                {
                    value_t pastSample = delay.readBackward(centerSample + w);
                    interp.write(w, pastSample);
                }
                oldSample = interp.read(delaySamples - centerSample);
            }
            else
            {
                // Faster linear resampler. Sounds fine to me.
                std::size_t t = static_cast<std::size_t>(std::floor(delaySamples));
                value_t s1 = delay.readBackward(t);
                value_t s2 = delay.readBackward(t+1);
                oldSample = LinearMix(delaySamples-t, s1, s2);
            }
            value_t feedbackSample = Compress(filtSample + resonance*oldSample);
            delay.write(feedbackSample);
            return feedbackSample;
        }
    };
}
