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


    void PhysicsMesh::SetStiffness(float _stiffness)
    {
        stiffness = std::max(0.0f, _stiffness);      // negative values would cause impossible & unstable physics
    }


    void PhysicsMesh::SetRestLength(float _restLength)
    {
        restLength = std::max(0.0f, _restLength);
    }


    int PhysicsMesh::Add(Ball ball)
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


    bool PhysicsMesh::Add(Spring spring)
    {
        const int nballs = static_cast<int>(currBallList.size());

        if (spring.ballIndex1 < 0 || spring.ballIndex1 >= nballs)
            return false;

        if (spring.ballIndex2 < 0 || spring.ballIndex2 >= nballs)
            return false;

        springList.push_back(spring);
        return true;
    }
}
