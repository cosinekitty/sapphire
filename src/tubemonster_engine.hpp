#pragma once

#include <stdexcept>
#include "tubeunit_engine.hpp"

namespace Sapphire
{
    class TubeMonsterEngine
    {
    private:
        float sampleRate = 0;

    public:
        void setSampleRate(float sampleRateHz)
        {
            sampleRate = sampleRateHz;
        }

        void process(float& leftOutput, float& rightOutput, float leftInput, float rightInput)
        {
            if (sampleRate <= 0.0f)
                throw std::logic_error("Invalid sample rate in TubeUnitEngine");

            // FIXFIXFIX do stuff here
            leftOutput = leftInput;
            rightOutput = rightInput;
        }
    };
}
