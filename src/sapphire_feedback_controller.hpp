#pragma once
#include <algorithm>
#include <cstdlib>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    // See the following for more info about proportional-integral feedback controllers:
    // https://apmonitor.com/pdc/index.php/Main/ProportionalIntegralControl


    template <typename value_t>
    struct FeedbackControllerResult
    {
        value_t response;
        bool    bounded;    // is the feedback controller operating within the bounds [-limit, +limit]?

        explicit FeedbackControllerResult(value_t _response, bool _bounded)
            : response(_response)
            , bounded(_bounded)
            {}
    };


    template <typename value_t>
    class FeedbackController
    {
    private:
        value_t limit = 100;        // output absolute-value limit, just like an op-amp saturating
        value_t kProportional;
        value_t kIntegral;
        value_t accum;              // integral sum
        value_t response;           // holds previous output for limit detection

        LoHiPassFilter<value_t> inFilter;
        LoHiPassFilter<value_t> outFilter;

        int unstableCountdown;

    public:
        FeedbackController()
        {
            setProportionalFactor();
            setIntegralFactor();
            initialize();
        }

        void initialize()
        {
            accum = 0;
            response = 0;
            inFilter.Reset();
            outFilter.Reset();
            unstableCountdown = 0;
        }

        void setProportionalFactor(value_t knob = 0)
        {
            value_t k = std::clamp(knob, static_cast<value_t>(-1), static_cast<value_t>(+1));
            kProportional = TenToPower<value_t>(2*(k + 0.15));
        }

        void setIntegralFactor(value_t knob = 0)
        {
            value_t k = std::clamp(knob, static_cast<value_t>(-1), static_cast<value_t>(+1));
            kIntegral = TenToPower<value_t>(-(k + 1.35));
        }

        FeedbackControllerResult<value_t> process(value_t error, float sampleRateHz)
        {
            inFilter.SetCutoffFrequency(100);
            inFilter.Update(error, sampleRateHz);
            value_t smooth = inFilter.LoPass();

            // Do not keep integrating error if we have saturated the output level.
            // This conditional logic is called "anti-reset windup".
            if (std::abs(response) < 0.98 * limit)
            {
                accum += smooth / (kIntegral * sampleRateHz);
                if (unstableCountdown > 0)
                    --unstableCountdown;
            }
            else
            {
                unstableCountdown = static_cast<int>(sampleRateHz / 10);
            }

            value_t rough = std::clamp(-kProportional*(smooth + accum), -limit, +limit);
            outFilter.SetCutoffFrequency(100);
            outFilter.Update(rough, sampleRateHz);
            response = outFilter.LoPass();
            return FeedbackControllerResult<value_t>(response, unstableCountdown==0);
        }
    };
}
