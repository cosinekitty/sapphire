#include <math.h>
#include "mesh.hpp"

namespace Sapphire
{
    double PhysicsVector::Magnitude() const
    {
        return sqrt(MagnitudeSquared());
    }


    void PhysicsMesh::Reset()
    {
        springList.clear();
        currBallList.clear();
        midBallList.clear();
        nextBallList.clear();
        forceList.clear();
        gravity = PhysicsVector();
        stiffness = DEFAULT_STIFFNESS;
        restLength = DEFAULT_REST_LENGTH;
        speedLimit = DEFAULT_SPEED_LIMIT;
    }


    void PhysicsMesh::SetStiffness(double _stiffness)
    {
        stiffness = std::max(0.0, _stiffness);      // negative values would cause impossible & unstable physics
    }


    void PhysicsMesh::SetRestLength(double _restLength)
    {
        restLength = std::max(0.0, _restLength);
    }


    int PhysicsMesh::Add(Ball ball)
    {
        int index = static_cast<int>(currBallList.size());

        // Save this ball in `ballList`.
        currBallList.push_back(ball);

        // Reserve a slot in the auxiliary arrays `midBallList` and `nextBallList`.
        midBallList.push_back(ball);
        nextBallList.push_back(ball);

        // Reserve a slot for calculating forces.
        forceList.push_back(PhysicsVector());

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
        BallList& blist,
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
            double dist = dr.Magnitude();   // length of the spring
            double attractiveForce = stiffness * (dist - restLength);
            if (dist < 1.0e-9)
            {
                // Think of this like two bullets hitting each other in a gunfight:
                // it should almost never happen.
                // The balls are so close together, it's hard to tell which direction the force should go.
                // We also risk dividing by zero.
                // It's a little weird/chaotic, but pick an arbitrary tension direction.
                force = PhysicsVector(0.0, 0.0, -attractiveForce);
            }
            else
            {
                force = (attractiveForce / dist) * dr;
            }

            if (b1.IsMobile())
                forceList[spring.ballIndex1] += force;

            if (b2.IsMobile())
                forceList[spring.ballIndex2] -= force;
        }
    }


    void PhysicsMesh::Dampen(BallList& blist, double dt, double halflife)
    {
        if (halflife > 0.0)
        {
            // damp^(frictionHalfLife/dt) = 0.5
            const double damp = pow(0.5, dt/halflife);
            for (Ball& b : blist)
                b.vel *= damp;
        }
    }


    void PhysicsMesh::Extrapolate(
        double dt,
        double speedLimit,
        const PhysicsVectorList& forceList,
        const BallList& sourceList,
        BallList& targetList)
    {
        const double speedLimitSquared = speedLimit * speedLimit;
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

                if (speedLimit > 0.0)
                {
                    double speedSquared = next.vel.MagnitudeSquared();
                    if (speedSquared > speedLimitSquared)
                        next.vel *= speedLimit / sqrt(speedSquared);
                }

                // Estimate the next position based on the average speed over the time increment.
                next.pos = curr.pos + ((dt / 2.0) * (curr.vel + next.vel));
            }
        }
    }


    void PhysicsMesh::Update(double dt, double halflife)
    {
        Dampen(currBallList, dt, halflife);
        CalcForces(currBallList, forceList);
        Extrapolate(dt / 2.0, speedLimit, forceList, currBallList, midBallList);
        CalcForces(midBallList, forceList);
        Extrapolate(dt, speedLimit, forceList, currBallList, nextBallList);
        Copy(nextBallList, currBallList);
    }


    void PhysicsMesh::Copy(const BallList& source, BallList& target)
    {
        const int nballs = static_cast<int>(source.size());
        for (int i = 0; i < nballs; ++i)
            target[i] = source[i];
    }


    HighPassFilter::HighPassFilter(double cutoffFrequencyHz)
        : first(true)
        , xprev(0.0)
        , yprev(0.0)
        , fc(cutoffFrequencyHz)
        {}

    double HighPassFilter::Update(double x, double sampleRateHz)
    {
        if (first)
        {
            first = false;
            xprev = x;
            yprev = x;
            return 0.0;
        }
        double c = sampleRateHz / (M_PI * fc);
        double y = (x + xprev - yprev*(1.0 - c)) / (1.0 + c);
        xprev = x;
        yprev = y;
        return x - y;
    }
}
