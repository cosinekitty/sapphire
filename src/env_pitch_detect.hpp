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

    public:
        EnvPitchDetector()
        {
            initialize();
        }

        void initialize()
        {
            for (int i = 0; i < maxChannels; ++i)
                prevFrame[i] = 0;
        }

        void process(int numChannels, const value_t* inFrame, value_t& outEnvelope, value_t& outPitchVoct)
        {
            assert(numChannels <= maxChannels);
            outEnvelope = 0;
            outPitchVoct = 0;
        }
    };
}
