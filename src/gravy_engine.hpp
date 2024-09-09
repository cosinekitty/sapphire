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
        const float DefaultResonanceKnob = 0.5;
        const float DefaultDriveKnob     = 0.5;
        const float DefaultMixKnob       = 1.0;
        const float DefaultGainKnob      = 0.5;

        template <int nchannels>
        class GravyEngine
        {
        private:
            bool dirty = true;
            float sampleRate = 0;
            FilterMode mode = FilterMode::Bandpass;
            float freqKnob  = DefaultFrequencyKnob;
            float resKnob   = DefaultResonanceKnob;
            float driveKnob = DefaultDriveKnob;
            float mixKnob   = DefaultMixKnob;
            float gainKnob  = DefaultGainKnob;

            BiquadFilter<float> filter[nchannels];

            float setKnob(float &v, float k, int lo = 0, int hi = 1)
            {
                if (std::isfinite(k))
                {
                    float knob = ClampInt(k, lo, hi);
                    if (knob != v)
                    {
                        dirty = true;
                        v = knob;
                    }
                }
                return v;
            }

            void configure(float sampleRateHz)
            {
                if (sampleRateHz != sampleRate)
                {
                    sampleRate = sampleRateHz;
                    dirty = true;
                }

                if (dirty)
                {
                    // Update filter parameters from knob values.
                    // This is more CPU expensive than running the filter itself.

                    float cornerFreqHz = std::pow(2.0f, freqKnob) * DefaultFrequencyHz;

                    float quality = std::pow(100.0f, resKnob);

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
                return setKnob(freqKnob, k, -OctaveRange, +OctaveRange);
            }

            float setResonance(float k)
            {
                return setKnob(resKnob, k);
            }

            float setDrive(float k)
            {
                return setKnob(driveKnob, k);
            }

            float setMix(float k)
            {
                return setKnob(mixKnob, k);
            }

            float setGain(float k)
            {
                return setKnob(gainKnob, k);
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

                for (int c = 0; c < nchannels; ++c)
                    outFrame[c] = filter[c].process(inFrame[c]);
            }
        };
    }
}
