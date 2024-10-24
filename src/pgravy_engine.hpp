#pragma once
#include "gravy_engine.hpp"

namespace Sapphire
{
    namespace Gravy
    {
        class SingleChannelGravyEngine
        {
        private:
            float freqKnob  = DefaultFrequencyKnob;
            float resKnob   = DefaultResonanceKnob;
            float mixKnob   = DefaultMixKnob;
            float gainKnob  = DefaultGainKnob;

            StateVariableFilter<float> filter;

            float setKnob(float &v, float k, int lo = 0, int hi = 1)
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

            float setFrequency(float k)
            {
                return setKnob(freqKnob, k, -OctaveRange, +OctaveRange);
            }

            float setResonance(float k)
            {
                return setKnob(resKnob, k);
            }

            float setMix(float k)
            {
                return setKnob(mixKnob, k);
            }

            float setGain(float k)
            {
                return setKnob(gainKnob, k);
            }

            FilterResult<float> process(float sampleRateHz, const float inSample)
            {
                float cornerFreqHz = std::pow(2.0f, freqKnob) * DefaultFrequencyHz;
                float gain  = Cube(gainKnob * 2);    // 0.5, the default value, should have unity gain.
                float mix = 1-Cube(1-mixKnob);

                FilterResult<float> result = filter.process(sampleRateHz, cornerFreqHz, resKnob, inSample);
                result.lowpass  = gain * (mix*result.lowpass  + (1-mix)*inSample);
                result.bandpass = gain * (mix*result.bandpass + (1-mix)*inSample);
                result.highpass = gain * (mix*result.highpass + (1-mix)*inSample);

                return result;
            }
        };
    }
}
