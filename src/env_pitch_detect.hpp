#pragma once
#include "sapphire_engine.hpp"

namespace Sapphire
{
    template <typename value_t, int maxChannels>
    class EnvPitchDetector
    {
    private:
        static_assert(maxChannels > 0);
        value_t prevFrame[maxChannels];
        int zeroCrossings[maxChannels];

        using filter_t = StagedFilter<float, 3>;
        filter_t loCutFilter[maxChannels] {};
        filter_t hiCutFilter[maxChannels] {};

    public:
        EnvPitchDetector()
        {
            initialize();
        }

        void initialize()
        {
            for (int c = 0; c < maxChannels; ++c)
            {
                prevFrame[c] = 0;
                zeroCrossings[c] = 0;

                loCutFilter[c].Reset();
                loCutFilter[c].SetCutoffFrequency(20);

                hiCutFilter[c].Reset();
                hiCutFilter[c].SetCutoffFrequency(3000);
            }
        }

        void process(int numChannels, float sampleRateHz, const value_t* inFrame, value_t& outEnvelope, value_t& outPitchVoct)
        {
            assert(numChannels <= maxChannels);
            outEnvelope = 0;
            outPitchVoct = 0;

            if (numChannels < 1)    // avoid division by zero later
                return;

            // Feed through a bandpass filter that rejects DC and other frequencies below 20 Hz,
            // and also rejects very high frequencies.
            for (int c = 0; c < numChannels; ++c)
            {
                float locut = loCutFilter[c].UpdateHiPass(inFrame[c], sampleRateHz);
                float bandpass = hiCutFilter[c].UpdateLoPass(locut, sampleRateHz);

                // Count zero-crossings (both ascending and descending), independently for each channel.

                if (bandpass * prevFrame[c] < 0)
                    ++zeroCrossings[c];

                prevFrame[c] = bandpass;
            }

            // HACK: for now output zero-crossings directly in outPitchVoct.
            // I just want to see what's happening.
            float sum = 0;
            for (int c = 0; c < numChannels; ++c)
                sum += zeroCrossings[c];
            outPitchVoct = sum / numChannels;
        }
    };
}
