/*
    nucleus_engine.hpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once

#include "sapphire_engine.hpp"

namespace Sapphire
{
    inline PhysicsVector EffectiveVelocity(const PhysicsVector& vel, float speedLimit)
    {
        float rawSpeed = Magnitude(vel);
        if (rawSpeed < speedLimit / 1.0e+6)
            return vel;     // prevent division by zero, or by very small numbers

        float effSpeed = BicubicLimiter(rawSpeed, speedLimit);
        return (effSpeed/rawSpeed) * vel;
    }

    struct Particle
    {
        PhysicsVector pos;
        PhysicsVector vel;
        PhysicsVector force;
        float mass = 1.0e-3f;
    };

    class NucleusEngine
    {
    private:
        const float max_dt = 0.0005f;
        std::vector<Particle> curr;
        std::vector<Particle> next;
        float magneticCoupling{};
        float speedLimit = 1000.0f;

        void calculateForces(std::vector<Particle>& array)
        {
            const float overlapDistance = 1.0e-4f;
            const int n = static_cast<int>(numParticles());

            // Reset all forces to zero, preparing to tally them.
            for (Particle& p : array)
                p.force = PhysicsVector::zero();

            // Find each pair of distinct particles (a, b).
            // Add up all the forces acting between the particles.
            for (int i = 0; i+1 < n; ++i)
            {
                Particle& a = array.at(i);
                for (int j = i+1; j < n; ++j)
                {
                    Particle& b = array.at(j);

                    // Find the mutual force vector between the two vectors.
                    // This includes an electrostatic component and a magnetic component.

                    // Calculate mutual quadrature = distance squared.
                    PhysicsVector dr = b.pos - a.pos;
                    float dist2 = Quadrature(dr);

                    // If two particles are very close to each other, consider them overlapping,
                    // and consider there to be zero force between them. This prevents division by
                    // zero (or by very small distances), resulting in destabilizing the simulation.
                    if (dist2 > overlapDistance * overlapDistance)
                    {
                        float dist = std::sqrt(dist2);
                        float dist3 = dist2 * dist;

                        // Use effective velocities, not raw velocities, to calculate magnetic cross product.
                        PhysicsVector av = EffectiveVelocity(a.vel, speedLimit);
                        PhysicsVector bv = EffectiveVelocity(b.vel, speedLimit);

                        // Calculate the magnetic component of the mutual force and include it in the vector sum.
                        PhysicsVector f = (dist - 1/dist3)*dr + (magneticCoupling / dist3)*Cross(bv - av, dr);

                        // Forces always act in equal and opposite pairs.
                        a.force += f;
                        b.force -= f;
                    }
                }
            }
        }

        void extrapolate(float dt)
        {
            const int n = static_cast<int>(numParticles());

            for (int i = 0; i < n; ++i)
            {
                const Particle& p1 = curr.at(i);
                Particle& p2 = next.at(i);

                // F = m*a  ==>  a = F/m.
                PhysicsVector acc = p1.force / p1.mass;

                // Estimate the net velocity change over the interval.
                PhysicsVector dV = dt * acc;

                // Calculate new position using mean velocity change over the interval.
                // We must use the bicubic limiter / "effective velocity" to avoid explosions.

                PhysicsVector v2 = p1.vel + dV/2;
                p2.pos = p1.pos + (dt * EffectiveVelocity(v2, speedLimit));

                // Calculate the velocity at the end of the time interval.
                p2.vel = p1.vel + dV;
            }
        }

        void step(float dt, float friction)
        {
            const int n = static_cast<int>(numParticles());

            // Calculate the forces at the existing configuration.
            calculateForces(curr);

            // Do a naive extrapolation to the midpoint of the time interval.
            // We assume the resulting configuration closely approximates the mean
            // conditions over the whole time interval.
            extrapolate(dt / 2);

            // Calculate forces after moving halfway.
            calculateForces(next);

            // Pretend like the midpoint forces apply at the beginning of the time interval.
            for (int i = 0; i < n; ++i)
                curr[i].force = next[i].force;

            // Extrapolate to the full time interval.
            extrapolate(dt);

            // Update the current state to the calcuted next state.
            // Apply friction at the same time.
            for (int i = 0; i < n; ++i)
            {
                curr[i] = next[i];
                curr[i].vel *= friction;
            }
        }

    public:
        explicit NucleusEngine(std::size_t _nParticles)
            : curr(_nParticles)
            , next(_nParticles)
            {}

        void setMagneticCoupling(float mc)
        {
            magneticCoupling = mc;
        }

        void update(float dt, float halflife)
        {
            // Use oversampling to keep the time increment within stability limits.
            int n = static_cast<int>(std::ceil(dt / max_dt));
            if (n < 1) n = 1;   // should never happen, but be careful
            const double et = dt / n;
            const float friction = std::pow(0.5, static_cast<double>(et)/halflife);
            for (int i = 0; i < n; ++i)
                step(et, friction);
        }

        std::size_t numParticles() const
        {
            return curr.size();
        }

        Particle& particle(int index)
        {
            return curr.at(index);
        }

        const Particle& particle(int index) const
        {
            return curr.at(index);
        }
    };
}
