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
                    bandpassFilter.centerFrequencyHz = C4_FREQUENCY_HZ;

                    notchFilter.initialize();
                    notchFilter.centerFrequencyHz = C4_FREQUENCY_HZ;

                    combFilter.initialize();
                    combFilter.centerFrequencyHz = C4_FREQUENCY_HZ;
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
                const float cascadeClamp = std::clamp<float>(cascade, 0, MAX_FILTER_STAGES);
                unsigned k = static_cast<unsigned>(std::floor(cascadeClamp));
                float fraction = cascadeClamp - k;
                if (k >= MAX_FILTER_STAGES)
                {
                    k = MAX_FILTER_STAGES - 1;
                    fraction = 1;
                }

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

                const iter_t& ik0 = iter.at(k+0);
                const iter_t& ik1 = iter.at(k+1);

                // CPU optimization: most of the time we are operating in
                // pure bandpass mode (modeMix == 0) or pure notch/comb mode (modeMix == 1).
                // Because changing modes is manual only, and there is a brief crossfade period,
                // I'm assuming that we don't have to worry about transient effects caused by
                // turning unused filters on/off.
                if (modeMix == 0)
                {
                    // While operating in a steady bandpass mode, only calculate the bandpass cascade.
                    for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                        iter[i+1].bandpass = multi[i].bandpassFilter.process(sampleRateHz, iter[i].bandpass).bandpass;  // cppcheck-suppress unreadVariable

                    return LinearMix(fraction, ik0.bandpass, ik1.bandpass);
                }

                if (modeMix == 1)
                {
                    // In a pure notch/comb mode, only calculate the notch and comb filters.
                    for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                    {
                        iter[i+1].notch = multi[i].notchFilter.process(sampleRateHz, iter[i].notch).notch;
                        iter[i+1].comb = multi[i].combFilter.process(sampleRateHz, iter[i].comb);
                    }

                    float yn = LinearMix(fraction, ik0.notch, ik1.notch);
                    float yc = LinearMix(fraction, ik0.comb, ik1.comb);
                    return LinearMix(resonanceKnob, yn, yc);
                }

                // While in transition, we have to calculate all filters and fade between them.
                for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                {
                    iter[i+1].bandpass = multi[i].bandpassFilter.process(sampleRateHz, iter[i].bandpass).bandpass;
                    iter[i+1].notch = multi[i].notchFilter.process(sampleRateHz, iter[i].notch).notch;
                    iter[i+1].comb = multi[i].combFilter.process(sampleRateHz, iter[i].comb);
                }

                // Return the linear interpolation between the outputs of the adjacent stages.
                float yb = LinearMix(fraction, ik0.bandpass, ik1.bandpass);
                float yn = LinearMix(fraction, ik0.notch, ik1.notch);
                float yc = LinearMix(fraction, ik0.comb, ik1.comb);
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
                constexpr float bandScale = 0.75;
                constexpr float notchScale = 0.25;
                constexpr float combScale = 0.6;

                resonanceKnob = std::clamp<float>(knob, 0, 1);
                for (MultiFilter& m : multi)
                {
                    m.bandpassFilter.setResonance(bandScale * resonanceKnob);
                    m.notchFilter.setResonance(notchScale * resonanceKnob);
                    m.combFilter.setResonance(combScale * resonanceKnob);
                }
            }

            void setInterpolator(InterpolatorKind interpKind)
            {
                for (MultiFilter& m : multi)
                    m.combFilter.interpKind = interpKind;
            }
        };
    }
}
