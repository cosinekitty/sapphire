#pragma once
#include <algorithm>
#include <cstdlib>

namespace Sapphire
{
    // See the following for more info about proportional-integral feedback controllers:
    // https://apmonitor.com/pdc/index.php/Main/ProportionalIntegralControl

    template <typename value_t>
    class FeedbackController
    {
    private:
        value_t limit = 100;        // output absolute-value limit, just like an op-amp saturating
        value_t vigor = 10;         // numerator for adjusting overall aggressiveness of response
        value_t inertia = 0.1;      // denominator for adjusting integral time constant
        value_t accum = 0;          // integral sum
        value_t response = 0;       // holds previous output for limit detection

    public:
        void initialize()
        {
            accum = 0;
            response = 0;
        }

        value_t process(value_t error, int sampleRateHz)
        {
            // Do not keep integrating error if we have saturated the output level.
            // This is called "anti-reset windup" in the literature.
            if (std::abs(response) < limit)
                accum += error / (inertia * sampleRateHz);

            response = std::clamp(vigor*(error + accum), -limit, +limit);
            return response;
        }
    };
}
