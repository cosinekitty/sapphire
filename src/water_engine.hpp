#ifndef __COSINEKITTY_WATER_ENGINE_HPP
#define __COSINEKITTY_WATER_ENGINE_HPP

// Sapphire water pool synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"
#include "waterpool.hpp"

namespace Sapphire
{
    const int WATERPOOL_WIDTH  = 24;
    const int WATERPOOL_HEIGHT = 24;

    class WaterEngine
    {
    private:
        WaterPoolSimd<WATERPOOL_WIDTH, WATERPOOL_HEIGHT> pool;
        int leftInputX;
        int leftInputY;
        int rightInputX;
        int rightInputY;
        int leftOutputX;
        int leftOutputY;
        int rightOutputX;
        int rightOutputY;
        float halflife;
        float propagation;

    public:
        WaterEngine()
        {
            initialize();
        }

        void initialize()
        {
            // Reset to initial state.
            pool.initialize();
            leftInputX = WATERPOOL_WIDTH/10;
            leftInputY = WATERPOOL_HEIGHT/3;
            rightInputX = WATERPOOL_WIDTH/10;
            rightInputY = (WATERPOOL_HEIGHT*2)/3;
            leftOutputX = (WATERPOOL_WIDTH*9)/10;
            leftOutputY = WATERPOOL_HEIGHT/3;
            rightOutputX = (WATERPOOL_WIDTH*9)/10;
            rightOutputY = (WATERPOOL_HEIGHT*2)/3;
            setHalfLife();
            setPropagation();
        }

        void setHalfLife(float h = -1.0f)
        {
            halflife = std::pow(10.0f, Clamp(h, -2.0f, +1.0f));
        }

        void setPropagation(float k = 7.0f)
        {
            propagation = std::pow(10.0f, Clamp(k, 5.0f, 9.0f));
        }

        void process(float dt, float& leftOutput, float& rightOutput, float leftInput, float rightInput)
        {
            // Feed input into the pool.
            pool.putPos(leftInputX,  leftInputY,  leftInput );
            pool.putPos(rightInputX, rightInputY, rightInput);

            // Update the simulation state.
            pool.update(dt, halflife, propagation);

            // Extract output from the pool.
            leftOutput  = pool.get(leftOutputX,  leftOutputY ).pos;
            rightOutput = pool.get(rightOutputX, rightOutputY).pos;
        }
    };
}

#endif // __COSINEKITTY_WATER_ENGINE_HPP