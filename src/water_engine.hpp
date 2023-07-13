#ifndef __COSINEKITTY_WATER_ENGINE_HPP
#define __COSINEKITTY_WATER_ENGINE_HPP

// Sapphire water pool synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"
#include "waterpool.hpp"

namespace Sapphire
{
    const int WATERPOOL_WIDTH  = 80;
    const int WATERPOOL_HEIGHT = 60;

    class WaterEngine
    {
    private:
        WaterPoolSimd<WATERPOOL_WIDTH, WATERPOOL_HEIGHT> pool;
        int leftInputX   {WATERPOOL_WIDTH/10};
        int leftInputY   {WATERPOOL_HEIGHT/3};
        int rightInputX  {WATERPOOL_WIDTH/10};
        int rightInputY  {(WATERPOOL_HEIGHT*2)/3};
        int leftOutputX  {(WATERPOOL_WIDTH*9)/10};
        int leftOutputY  {WATERPOOL_HEIGHT/3};
        int rightOutputX {(WATERPOOL_WIDTH*9)/10};
        int rightOutputY {(WATERPOOL_HEIGHT*2)/3};
        float halflife   {0.3f};
        float propagation {250000.0f};

    public:
        void setHalfLife(float newHalfLife)
        {
            halflife = Clamp(halflife, 0.01f, 10.0f);
        }

        void setPropagation(float k)
        {
            propagation = Clamp(k, 1.0f, 1.0e+9f);
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
