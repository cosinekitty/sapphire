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

        template <int maxchannels>
        class GravyEngine
        {
        private:
            static_assert(maxchannels > 0);
            static constexpr int maxquads = (maxchannels + 3) / 4;

            FilterMode mode = FilterMode::Bandpass;

            float freqKnob  = DefaultFrequencyKnob;
            float resKnob   = DefaultResonanceKnob;
            float mixKnob   = DefaultMixKnob;
            float gainKnob  = DefaultGainKnob;

            StateVariableFilter<PhysicsVector> filter[maxquads];

            float setKnob(float &v, float k, int lo = 0, int hi = 1)
            {
                if (std::isfinite(k))
                {
                    v = ClampInt(k, lo, hi);
                }
                return v;
            }

        public:
            GravyEngine()
            {
                initialize();
            }

            void initialize()
            {
                for (int q = 0; q < maxquads; ++q)
                    filter[q].initialize();
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

            void setFilterMode(FilterMode m)
            {
                mode = m;
            }

            int process(float sampleRateHz, int nchannels, const float inFrame[], float outFrame[])
            {
                if (nchannels < 1)
                    return 0;

                if (nchannels > maxchannels)
                    nchannels = maxchannels;

                const int nquads = (nchannels + 3) / 4;
                float cornerFreqHz = std::pow(2.0f, freqKnob) * DefaultFrequencyHz;
                float gain  = Cube(gainKnob * 2);    // 0.5, the default value, should have unity gain.
                float mix = 1-Cube(1-mixKnob);
                PhysicsVector x;
                int c = 0;
                for (int q = 0; q < nquads; ++q, c += 4)
                {
                    x[0] = (c+0 < nchannels) ? inFrame[c+0] : 0;
                    x[1] = (c+1 < nchannels) ? inFrame[c+1] : 0;
                    x[2] = (c+2 < nchannels) ? inFrame[c+2] : 0;
                    x[3] = (c+3 < nchannels) ? inFrame[c+3] : 0;

                    FilterResult<PhysicsVector> result = filter[q].process(sampleRateHz, cornerFreqHz, resKnob, x);
                    PhysicsVector y = result.select(mode);
                    PhysicsVector f = gain * (mix*y + (1-mix)*x);

                    if (c+0 < nchannels) outFrame[c+0] = f[0];
                    if (c+1 < nchannels) outFrame[c+1] = f[1];
                    if (c+2 < nchannels) outFrame[c+2] = f[2];
                    if (c+3 < nchannels) outFrame[c+3] = f[3];
                }

                return nchannels;
            }
        };
    }
}
