#pragma once

#include <stdexcept>
#include "tubeunit_engine.hpp"

namespace Sapphire
{
    class TubeMonsterEngine
    {
    private:
        float sampleRate = 0;
        TubeUnitEngine tube;

    public:
        TubeMonsterEngine()
        {
            initialize();
        }

        void setSampleRate(float sampleRateHz)
        {
            sampleRate = sampleRateHz;
            tube.setSampleRate(sampleRateHz);
        }

        void initialize()
        {
            tube.initialize();
            tube.setRootFrequency(32.0f);
        }

        void process(float& leftOutput, float& rightOutput, float leftInput, float rightInput)
        {
            tube.process(leftOutput, rightOutput, leftInput, rightInput);
        }
    };
}
