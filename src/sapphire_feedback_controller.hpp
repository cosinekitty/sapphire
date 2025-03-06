#pragma once
#include <algorithm>
#include <cstdlib>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    // See the following for more info about proportional-integral feedback controllers:
    // https://apmonitor.com/pdc/index.php/Main/ProportionalIntegralControl

    template <typename value_t>
    class FeedbackController
    {
    private:
        value_t limit = 100;        // output absolute-value limit, just like an op-amp saturating
        value_t vigor = 50;         // numerator for adjusting overall aggressiveness of response
        value_t inertia = 1;        // denominator for adjusting integral time constant
        value_t accum;              // integral sum
        value_t response;           // holds previous output for limit detection

        LoHiPassFilter<value_t> inFilter;
        LoHiPassFilter<value_t> outFilter;

    public:
        FeedbackController()
        {
            initialize();
        }

        void initialize()
        {
            accum = 0;
            response = 0;
            inFilter.Reset();
            outFilter.Reset();
        }

        value_t process(value_t error, float sampleRateHz)
        {
            inFilter.SetCutoffFrequency(100);
            inFilter.Update(error, sampleRateHz);
            value_t smooth = inFilter.LoPass();

            // Do not keep integrating error if we have saturated the output level.
            // This conditional logic is called "anti-reset windup".
            if (std::abs(response) < limit)
                accum += smooth / (inertia * sampleRateHz);

            value_t rough = std::clamp(vigor*(smooth + accum), -limit, +limit);
            outFilter.SetCutoffFrequency(100);
            outFilter.Update(rough, sampleRateHz);
            response = outFilter.LoPass();
            return response;
        }
    };
}
