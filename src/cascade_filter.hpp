#pragma once
#include <array>
#include <cmath>
#include "sauce_engine.hpp"
#include "comb_filter.hpp"

namespace Sapphire
{
    namespace Empath
    {
        template <typename value_t, unsigned MAX_FILTER_STAGES>
        class CascadeFilter
        {
            static_assert(MAX_FILTER_STAGES > 0);

        public:
            using filter_t = Gravy::SingleChannelGravyEngine<value_t>;
            using comb_t = CombFilter<value_t>;

        private:
            struct MultiFilter
            {
                filter_t    bandpassFilter;
                filter_t    notchFilter;
                comb_t      combFilter;

                void initialize()
                {
                    bandpassFilter.initialize();
                    notchFilter.initialize();
                    combFilter.initialize();
                }
            };

            std::array<MultiFilter, MAX_FILTER_STAGES> multi;

        public:
            void initialize()
            {
                for (MultiFilter& m : multi)
                    m.initialize();
            }

            value_t process(float sampleRateHz, value_t inSample, float cascade, float modeMix)
            {
                const float c = std::clamp<float>(cascade, 0, MAX_FILTER_STAGES);

                struct iter_t
                {
                    value_t bandpass{};
                    value_t notch{};
                    value_t comb{};

                    void setDrySample(value_t dry)
                    {
                        bandpass = dry;
                        notch = dry;
                        comb = dry;
                    }
                };

                std::array<iter_t, 1+MAX_FILTER_STAGES> iter;
                iter[0].setDrySample(inSample);

                for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                {
                    iter[i+1].bandpass = multi[i].bandpassFilter.process(sampleRateHz, iter[i].bandpass).bandpass;
                    iter[i+1].notch = multi[i].notchFilter.process(sampleRateHz, iter[i].notch).notch;
                }

                // Return the linear interpolation between the outputs of the adjacent stages.
                unsigned k = static_cast<unsigned>(std::floor(c));
                float m = c - k;
                float yb = LinearMix(m, iter[k].bandpass, iter[k+1].bandpass);
                float yn = LinearMix(m, iter[k].notch, iter[k+1].notch);
                return LinearMix(modeMix, yb, yn);
            }

            void setFrequency(float knob)
            {
                for (auto& m : multi)
                {
                    m.bandpassFilter.setFrequency(knob);
                    m.notchFilter.setFrequency(knob);
                    // comb
                }
            }

            void setResonance(float knob)
            {
                for (auto& m : multi)
                {
                    m.bandpassFilter.setResonance(knob);
                    m.notchFilter.setResonance(knob);
                    // comb
                }
            }
        };
    }
}
