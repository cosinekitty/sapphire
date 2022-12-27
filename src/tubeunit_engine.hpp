#ifndef __COSINEKITTY_TUBEUNIT_ENGINE_HPP
#define __COSINEKITTY_TUBEUNIT_ENGINE_HPP

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"

namespace Sapphire
{
    class TubeUnitEngine
    {
    private:

    public:
        TubeUnitEngine()
        {
            initialize();
        }

        void initialize()
        {
        }

        void process(float sampleRate, float& leftOutput, float& rightOutput)
        {
            (void)sampleRate;
            leftOutput = 0.0f;
            rightOutput = 0.0f;
        }
    };
}

#endif // __COSINEKITTY_TUBEUNIT_ENGINE_HPP
