#pragma once
#include "gravy_engine.hpp"

namespace Sapphire
{
    namespace Gravy
    {
        template <typename value_t>
        class SingleChannelGravyEngine
        {
        private:
            value_t freqKnob  = DefaultFrequencyKnob;
            value_t resKnob   = DefaultResonanceKnob;
            value_t mixKnob   = DefaultMixKnob;
            value_t gainKnob  = DefaultGainKnob;

            StateVariableFilter<value_t> filter;

            float setKnob(value_t &v, value_t k, int lo = 0, int hi = 1)
            {
                if (std::isfinite(k))
                {
                    v = ClampInt(k, lo, hi);
                }
                return v;
            }

        public:
            SingleChannelGravyEngine()
            {
                initialize();
            }

            void initialize()
            {
                filter.initialize();
            }

            value_t setFrequency(value_t k)
            {
                return setKnob(freqKnob, k, -OctaveRange, +OctaveRange);
            }

            value_t setResonance(value_t k)
            {
                return setKnob(resKnob, k);
            }

            value_t setMix(value_t k)
            {
                return setKnob(mixKnob, k);
            }

            value_t setGain(value_t k)
            {
                return setKnob(gainKnob, k);
            }

            FilterResult<value_t> process(value_t sampleRateHz, const value_t inSample)
            {
                value_t cornerFreqHz = TwoToPower(freqKnob) * DefaultFrequencyHz;
                value_t gain  = Cube(gainKnob * 2);    // 0.5, the default value, should have unity gain.
                value_t mix = 1-Cube(1-mixKnob);

                FilterResult<value_t> result = filter.process(sampleRateHz, cornerFreqHz, resKnob, inSample);
                result.lowpass  = gain * (mix*result.lowpass  + (1-mix)*inSample);
                result.bandpass = gain * (mix*result.bandpass + (1-mix)*inSample);
                result.highpass = gain * (mix*result.highpass + (1-mix)*inSample);

                return result;
            }
        };
    }
}
