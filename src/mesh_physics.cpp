#include <math.h>
#include <cstddef>
#include "sapphire_engine.hpp"
#include "elastika_engine.hpp"

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    void PhysicsMesh::Clear()
    {
        springList.clear();
        currBallList.clear();
        nextBallList.clear();
        originalPositions.clear();
        forceList.clear();
        gravity = PhysicsVector::zero();
        magnet = PhysicsVector::zero();
        stiffness  = MESH_DEFAULT_STIFFNESS;
        restLength = MESH_DEFAULT_REST_LENGTH;
        speedLimit = MESH_DEFAULT_SPEED_LIMIT;
    }


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


    void PhysicsMesh::CalcForces(
        const BallList& blist,
        PhysicsVectorList& forceList)
    {
        // Start with gravity acting on all balls. (But only effective on mobile balls.)
        const int nballs = static_cast<int>(blist.size());
        for (int i = 0; i < nballs; ++i)
            if (blist[i].IsMobile())
                forceList[i] = blist[i].mass * gravity;

        // Calculate the force caused on balls by the tension in each spring.
        // Add equal and opposite force vectors to the pair of attached balls.
        PhysicsVector force;
        for (const Spring& spring : springList)
        {
            // dr = vector from ball 1 toward ball 2.
            const Ball& b1 = blist[spring.ballIndex1];
            const Ball& b2 = blist[spring.ballIndex2];
            PhysicsVector dr = b2.pos - b1.pos;
            float dist = Magnitude(dr);   // length of the spring
            float attractiveForce = stiffness * (dist - restLength);
            if (dist < 1.0e-9f)
            {
                // Think of this like two bullets hitting each other in a gunfight:
                // it should almost never happen.
                // The balls are so close together, it's hard to tell which direction the force should go.
                // We also risk dividing by zero.
                // It's a little weird/chaotic, but pick an arbitrary tension direction.
                force = PhysicsVector(0.f, 0.f, -attractiveForce, 0.f);
            }
            else
            {
                force = (attractiveForce / dist) * dr;
            }

            if (b1.IsMobile())
            {
                forceList[spring.ballIndex1] += force;
                forceList[spring.ballIndex1] += Cross(b1.vel, magnet);
            }

            if (b2.IsMobile())
            {
                forceList[spring.ballIndex2] -= force;
                forceList[spring.ballIndex2] += Cross(b2.vel, magnet);
            }
        }
    }


    void PhysicsMesh::Dampen(BallList& blist, float dt, float halflife)
    {
        // damp^(frictionHalfLife/dt) = 0.5.
        const float damp = std::pow(0.5f, dt/halflife);
        for (Ball& b : blist)
            b.vel *= damp;
    }


    void PhysicsMesh::Extrapolate(
        float dt,
        float speedLimit,
        const PhysicsVectorList& forceList,
        const BallList& sourceList,
        BallList& targetList)
    {
        const float speedLimitSquared = speedLimit * speedLimit;
        const int nballs = static_cast<int>(sourceList.size());
        for (int i = 0; i < nballs; ++i)
        {
            const Ball& curr = sourceList[i];
            Ball& next = targetList[i];
            if (curr.IsAnchor())
            {
                // This is an "anchor" that never moves, not a normal ball.
                next = curr;
            }
            else
            {
                // It is possible for the caller to modify a ball's mass.
                // Make sure we keep masses in sync.
                next.mass = curr.mass;

                // Update the velocity vector from `curr` into `next`.
                next.vel = curr.vel + ((dt / curr.mass) * forceList[i]);

                if (speedLimit > 0)
                {
                    float speedSquared = Dot(next.vel, next.vel);
                    if (speedSquared > speedLimitSquared)
                        next.vel *= speedLimit / std::sqrt(speedSquared);
                }

                // Estimate the next position based on the average speed over the time increment.
                next.pos = curr.pos + ((dt / 2) * (curr.vel + next.vel));
            }
        }
    }


    void PhysicsMesh::Update(float dt, float halflife)
    {
        Dampen(currBallList, dt, halflife);
        CalcForces(currBallList, forceList);
        Extrapolate(dt/2, speedLimit, forceList, currBallList, nextBallList);
        CalcForces(nextBallList, forceList);
        Extrapolate(dt, speedLimit, forceList, currBallList, nextBallList);
        std::swap(nextBallList, currBallList);
    }
}
