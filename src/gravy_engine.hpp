#pragma once
#include <algorithm>
#include "sapphire_engine.hpp"


namespace Sapphire
{
    namespace Gravy
    {
        const int OctaveRange = 5;                              // +/- octave range around default frequency
        const float DefaultFrequencyHz = 523.2511306011972;     // C5 = 440*(2**0.25)
        const int FrequencyFactor = 1 << OctaveRange;
        //const float MinFrequencyHz = DefaultFrequencyHz / FrequencyFactor;
        //const float MaxFrequencyHz = DefaultFrequencyHz * FrequencyFactor;

        const float DefaultFrequencyKnob = 0.0;
        const float DefaultResonanceKnob = 0.0;
        const float DefaultMixKnob       = 1.0;
        const float DefaultGainKnob      = 0.5;

        template <int nchannels>
        class GravyEngine
        {
        private:
            bool dirty = true;
            float sampleRate = 0;
            FilterMode mode = FilterMode::Bandpass;

            // Slow knobs: require setting `dirty` flag when changed.
            float freqKnob  = DefaultFrequencyKnob;
            float resKnob   = DefaultResonanceKnob;

            // Fast knobs: simple scalar factors.
            float mixKnob   = DefaultMixKnob;
            float gainKnob  = DefaultGainKnob;

            BiquadFilter<float> filter[nchannels];

            float setFastKnob(float &v, float k, int lo = 0, int hi = 1)
            {
                // A "fast knob" is a control that we can happily calculate
                // at audio rates. We have mix and gain, each of which
                // is just a scalar factor; these multiplications consume negligible CPU.
                if (std::isfinite(k))
                {
                    v = ClampInt(k, lo, hi);
                }
                return v;
            }

            float setSlowKnob(float &v, float k, int lo = 0, int hi = 1)
            {
                // A "slow knob" is a control that causes higher CPU overhead when changed.
                // All slow knobs set the `dirty` flag when their value changes.
                // This speeds up cases where parameters are not changing very often.
                // Slow knobs include frequency and resonance (quality).
                // Any time one or more of the "slow knobs" is changed, we have to
                // do an expensive recalculation of filter parameters.
                // We prefer to set a dirty flag and defer recalculating the filter coefficients
                // until we process the next audio frame.
                if (std::isfinite(k))
                {
                    float prev = v;
                    v = ClampInt(k, lo, hi);
                    if (!std::isfinite(prev) || v != prev)
                        dirty = true;
                }
                return v;
            }

            void configure(float sampleRateHz)
            {
                if (dirty || (sampleRateHz != sampleRate))
                {
                    sampleRate = sampleRateHz;

                    // Update filter parameters from knob values.
                    // This is more CPU expensive than running the filter itself.

                    float cornerFreqHz = std::pow(2.0f, freqKnob) * DefaultFrequencyHz;

                    float quality = std::pow(50.0f, resKnob);

                    // FIXFIXFIX: replace with SSE and do 4 channels for the price of 1.
                    // Even though I only need 2 of those 4 channels, it's still faster
                    // than what I'm doing now with this `for` loop.

                    for (int c = 0; c < nchannels; ++c)
                        filter[c].configure(mode, sampleRate, cornerFreqHz, quality);

                    dirty = false;
                }
            }

        public:
            GravyEngine()
            {
                initialize();
            }

            void initialize()
            {
                for (int c = 0; c < nchannels; ++c)
                    filter[c].initialize();

                sampleRate = 0;
                dirty = true;
            }

            float setFrequency(float k)
            {
                return setSlowKnob(freqKnob, k, -OctaveRange, +OctaveRange);
            }

            float setResonance(float k)
            {
                return setSlowKnob(resKnob, k);
            }

            float setMix(float k)
            {
                return setFastKnob(mixKnob, k);
            }

            float setGain(float k)
            {
                return setFastKnob(gainKnob, k);
            }

            void setFilterMode(FilterMode m)
            {
                if (m != mode)
                {
                    dirty = true;
                    mode = m;
                }
            }

            void process(float sampleRateHz, const float inFrame[nchannels], float outFrame[nchannels])
            {
                configure(sampleRateHz);

                float gain  = Cube(gainKnob  * 2);    // 0.5, the default value, should have unity gain.
                float mix = mixKnob;
                for (int c = 0; c < nchannels; ++c)
                {
                    float x = inFrame[c];
                    float y = filter[c].process(x);
                    outFrame[c] = gain * (mix*y + (1-mix)*x);
                }
            }
        };
    }
}
