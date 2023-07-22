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
        float agitator;
        float detectorRadius;

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
            setDetectorRadius();
            agitator = 0.0f;
        }

        void setHalfLife(float h = -1.0f)
        {
            halflife = std::pow(10.0f, Clamp(h, -3.0f, +1.0f));
        }

        void setPropagation(float k = 7.0f)
        {
            propagation = std::pow(10.0f, Clamp(k, 5.0f, 9.0f));
        }

        void setDetectorRadius(float r = -3.0f)
        {
            detectorRadius = std::pow(10.0f, Clamp(r, -5.0f, -1.0f));
        }

        void process(float dt, float& leftOutput, float& rightOutput, float leftInput, float rightInput)
        {
            // Feed input into the pool.
            pool.pos(leftInputX,  leftInputY ) = leftInput;
            pool.pos(rightInputX, rightInputY) = rightInput;

            // Update the water state.
            pool.update(dt, halflife, propagation);

            // Extract output from the pool.
            leftOutput  = pool.pos(leftOutputX,  leftOutputY );
            rightOutput = pool.pos(rightOutputX, rightOutputY);

            // Calculate a gate signal based on whether a simulated reflection
            // of a laser beam from a specific location on the surface falls inside
            // a designated target area.
            float lx = leftOutput - pool.pos(leftOutputX-1, leftOutputY);
            float ly = leftOutput - pool.pos(leftOutputX, leftOutputY-1);
            if (lx*lx + ly*ly < detectorRadius*detectorRadius)
                agitator = 10.0f;
            else
                agitator = 0.0f;
        }

        float getAgitator() const
        {
            return agitator;
        }
    };
}

#endif // __COSINEKITTY_WATER_ENGINE_HPP
