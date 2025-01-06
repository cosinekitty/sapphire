#include <math.h>
#include <cstddef>
#include "sapphire_engine.hpp"
#include "elastika_engine.hpp"

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    void PhysicsMesh::Quiet()
    {
        const std::size_t nballs = currBallList.size();
        assert(nballs == originalPositions.size());
        for (std::size_t i = 0; i < nballs; ++i)
        {
            currBallList[i].pos = originalPositions.at(i);
            currBallList[i].vel = PhysicsVector::zero();
        }
    }

    int PhysicsMesh::AddBall(Ball ball)
    {
        int index = static_cast<int>(currBallList.size());

        // Save this ball in `ballList`.
        currBallList.push_back(ball);

        // Reserve a slot in the auxiliary array `nextBallList`.
        nextBallList.push_back(ball);

        // Reserve a slot for calculating forces.
        forceList.push_back(PhysicsVector::zero());

        // Remember where each ball started, so we can put it back.
        // This also provides a way to calculate the offset of a ball from its original position.
        originalPositions.push_back(ball.pos);

        return index;
    }
}
