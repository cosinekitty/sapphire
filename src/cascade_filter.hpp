#pragma once
#include <array>
#include <cmath>
#include "sauce_engine.hpp"
#include "comb_filter.hpp"

namespace Sapphire
{
    namespace Empath
    {
        template <unsigned MAX_FILTER_STAGES>
        class CascadeFilter
        {
        public:
            using filter_t = Gravy::SingleChannelGravyEngine<float>;
            using comb_t = CombFilter<float>;

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

            static_assert(MAX_FILTER_STAGES > 0);
            std::array<MultiFilter, MAX_FILTER_STAGES> multi;
            float resonanceKnob{};

        public:
            void initialize()
            {
                for (MultiFilter& m : multi)
                    m.initialize();
            }

            float process(float sampleRateHz, float inSample, float cascade, float modeMix)
            {
                const float c = std::clamp<float>(cascade, 0, MAX_FILTER_STAGES);

                struct iter_t
                {
                    float bandpass{};
                    float notch{};
                    float comb{};

                    void setDrySample(float dry)
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
                    iter[i+1].comb = multi[i].combFilter.process(sampleRateHz, iter[i].comb);
                }

                // Return the linear interpolation between the outputs of the adjacent stages.
                unsigned k = static_cast<unsigned>(std::floor(c));
                float m = c - k;
                float yb = LinearMix(m, iter[k].bandpass, iter[k+1].bandpass);
                float yn = LinearMix(m, iter[k].notch, iter[k+1].notch);
                float yc = LinearMix(m, iter[k].comb, iter[k+1].comb);
                float ync = LinearMix(resonanceKnob, yn, yc);
                return LinearMix(modeMix, yb, ync);
            }

            void setFrequency(float knob)
            {
                for (MultiFilter& m : multi)
                {
                    m.bandpassFilter.setFrequency(knob);
                    m.notchFilter.setFrequency(knob);
                    m.combFilter.setFrequency(knob);
                }
            }

            void setResonance(float knob)
            {
                constexpr float combScale = 0.6;

                resonanceKnob = std::clamp<float>(knob, 0, 1);
                for (MultiFilter& m : multi)
                {
                    m.bandpassFilter.setResonance(resonanceKnob);
                    m.notchFilter.setResonance(resonanceKnob);
                    m.combFilter.setResonance(combScale * resonanceKnob);        // FIXFIXFIX: also allow negative resonance
                }
            }
        };
    }
}
