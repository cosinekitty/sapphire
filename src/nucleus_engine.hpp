/*
    nucleus_engine.hpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once

#include "sapphire_engine.hpp"

namespace Sapphire
{
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
        std::vector<Particle> curr;
        std::vector<Particle> next;
        float magneticCoupling{};

        void calculateForces(std::vector<Particle>& array)
        {
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
                    float dist = std::sqrt(dist2);
                    float dist3 = dist2 * dist;

                    // Calculate the magnetic component of the mutual force and include it in the vector sum.
                    PhysicsVector f = (dist - 1/dist3)*dr + (magneticCoupling / dist3)*Cross(b.vel - a.vel, dr);

                    // Forces always act in equal and opposite pairs.
                    a.force += f;
                    b.force -= f;
                }
            }
        }

        void extrapolate(float dt)
        {
            const int n = static_cast<int>(numParticles());

            for (int i = 0; i < n; ++i)
            {
                const Particle& a = curr.at(i);
                Particle& b = next.at(i);

                // F = m*a  ==>  a = F/m.
                PhysicsVector acc = a.force / a.mass;

                // Estimate the net velocity change over the interval.
                PhysicsVector dV = dt * acc;

                // Calculate new position using mean velocity change over the interval.
                b.pos = a.pos + (dt * (a.vel + dV/2));

                // Calculate the velocity at the end of the time interval.
                b.vel = a.vel + dV;
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

        void update(float dt, float halflife)
        {
            const float friction = std::pow(0.5, static_cast<double>(dt)/halflife);

            // FIXFIXFIX - Split into smaller time increments when needed for accuracy and stability.
            step(dt, friction);
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
