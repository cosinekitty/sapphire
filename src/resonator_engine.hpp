#ifndef __COSINEKITTY_RESONATOR_ENGINE_HPP
#define __COSINEKITTY_RESONATOR_ENGINE_HPP

#include "sapphire_engine.hpp"

namespace Sapphire
{
    class ResonatorEngine
    {
    private:

    public:
        ResonatorEngine()
        {
            initialize();
        }

        void initialize()
        {
        }

        void process(float sampleRate, float leftIn, float rightIn, float& leftOut, float& rightOut)
        {
            leftOut = 0.0f;
            rightOut = 0.0f;
        }
    };
}

#endif // __COSINEKITTY_RESONATOR_ENGINE_HPP
