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

            float process(float sampleRateHz, float inSample, float cascade, float morph)
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
                float yb = (1-m)*bandpass[k] + m*bandpass[k+1];
                float yn = (1-m)*notch[k] + m*notch[k+1];

                const float z = (morph+1)/2;    // convert range [-1, +1] to [0, 1].
                return (1-z)*yn + z*yb;
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
