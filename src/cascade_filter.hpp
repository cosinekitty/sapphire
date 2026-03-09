#pragma once
#include <array>
#include <cmath>
#include "sauce_engine.hpp"

namespace Sapphire
{
    namespace Empath
    {
        template <typename value_t, unsigned MAX_FILTER_STAGES>
        class CascadeFilter
        {
            static_assert(MAX_FILTER_STAGES > 0);

        public:
            using result_t = FilterResult<value_t>;
            using filter_t = Gravy::SingleChannelGravyEngine<value_t>;

        private:
            std::array<filter_t, MAX_FILTER_STAGES> bandpassStage;
            std::array<filter_t, MAX_FILTER_STAGES> notchStage;

        public:
            void initialize()
            {
                for (filter_t& f : bandpassStage)
                    f.initialize();

                for (filter_t& f : notchStage)
                    f.initialize();
            }

            float process(float sampleRateHz, float inSample, float cascade, float modeMix)
            {
                const float c = std::clamp<float>(cascade, 0, MAX_FILTER_STAGES);

                float bandpass[1+MAX_FILTER_STAGES];
                float notch[1+MAX_FILTER_STAGES];

                bandpass[0] = notch[0] = inSample;

                for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                {
                    bandpass[i+1] = bandpassStage[i].process(sampleRateHz, bandpass[i]).bandpass;
                    notch[i+1] = notchStage[i].process(sampleRateHz, notch[i]).notch;
                }

                // Return the linear interpolation between the outputs of the adjacent stages.
                unsigned k = static_cast<unsigned>(std::floor(c));
                float m = c - k;
                float yb = LinearMix(m, bandpass[k], bandpass[k+1]);
                float yn = LinearMix(m, notch[k], notch[k+1]);
                return LinearMix(modeMix, yb, yn);
            }

            void setFrequency(float knob)
            {
                for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                {
                    bandpassStage[i].setFrequency(knob);
                    notchStage[i].setFrequency(knob);
                }
            }

            void setResonance(float knob)
            {
                for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                {
                    bandpassStage[i].setResonance(knob);
                    notchStage[i].setResonance(knob);
                }
            }
        };
    }
}
