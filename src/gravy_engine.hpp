#pragma once
#include "sapphire_engine.hpp"


namespace Sapphire
{
    namespace Gravy
    {
        const int OctaveRange = 5;                              // +/- octave range around default frequency
        //const float DefaultFrequencyHz = 523.2511306011972;     // C5 = 440*(2**0.25)
        //const int FrequencyFactor = 1 << OctaveRange;
        //const float MinFrequencyHz = DefaultFrequencyHz / FrequencyFactor;
        //const float MaxFrequencyHz = DefaultFrequencyHz * FrequencyFactor;

        const float DefaultFrequencyKnob = 0.0;
        const float DefaultResonanceKnob = 0.5;
        const float DefaultDriveKnob     = 0.5;
        const float DefaultMixKnob       = 1.0;
        const float DefaultGainKnob      = 0.5;

        template <int nchannels>
        class PolyEngine
        {
        private:
            float freqKnob{};
            float resKnob{};
            float driveKnob{};
            float mixKnob{};
            float gainKnob{};

            BiquadFilter<float> filter[nchannels];

        public:
            PolyEngine()
            {
                initialize();
            }

            void initialize()
            {
                freqKnob  = DefaultFrequencyKnob;
                resKnob   = DefaultResonanceKnob;
                driveKnob = DefaultDriveKnob;
                mixKnob   = DefaultMixKnob;
                gainKnob  = DefaultGainKnob;

                for (int c = 0; c < nchannels; ++c)
                    filter[c].initialize();
            }
        };
    }
}
