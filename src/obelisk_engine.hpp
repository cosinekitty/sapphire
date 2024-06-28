/*
    obelisk_engine.hpp   -   Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once
#include <vector>
#include <stdexcept>
#include "sapphire_simd.hpp"
#include "sapphire_integrator.hpp"

namespace Sapphire
{
    namespace Obelisk
    {
        const float InitialParticleSpacingMeters = 1.0e-3;
        const float InitialParticleMass = 1.0e-3;

        template <int nparticles>
        struct GroupVector
        {
            PhysicsVector array[nparticles];

            GroupVector()
            {
            }

            explicit GroupVector(float init)
            {
                if (init != 0)
                    for (int i = 0; i < nparticles; ++i)
                        array[i] = init;
            }

            GroupVector<nparticles> operator + (const GroupVector<nparticles>& other)
            {
                GroupVector<nparticles> sum;
                for (int i = 0; i < nparticles; ++i)
                    sum.array[i] = array[i] + other.array[i];
                return sum;
            }

            GroupVector& operator += (const GroupVector<nparticles>& other)
            {
                for (int i = 0; i < nparticles; ++i)
                    array[i] += other.array[i];
                return *this;
            }
        };


        template <int nparticles>
        inline GroupVector<nparticles> operator * (float scalar, const GroupVector<nparticles>& group)
        {
            GroupVector<nparticles> product;
            for (int i = 0; i < nparticles; ++i)
                product.array[i] = scalar * group.array[i];
            return product;
        }


        template <int nparticles>
        class Engine
        {
            static_assert(nparticles >= 3, "The number of particles must be 3 or greater.");
        public:
            using group_vector_t = GroupVector<nparticles>;
            using state_vector_t = Integrator::StateVector<group_vector_t>;

            Engine()
                : accel_lambda([this] (const group_vector_t& r, const group_vector_t& v)
                    -> group_vector_t{ return acceleration(r, v); })
            {
                initialize();
            }

            void initialize()
            {
                state_vector_t state;
                for (int i = 0; i < nparticles; ++i)
                    state.r.array[i][0] = i * InitialParticleSpacingMeters;
                integrator.setState(state);
            }

            PhysicsVector& position(int index)
            {
                validateIndex(index);
                return integrator.state.r.array[index];
            }

            PhysicsVector& velocity(int index)
            {
                validateIndex(index);
                return integrator.state.v.array[index];
            }

            void process(float sampleRate)
            {
                integrator.update(1/sampleRate, accel_lambda);
            }

        private:
            float mass = 1.0e-3;
            float restLength = InitialParticleSpacingMeters / 4;
            float stiffness = 50000.0;
            Integrator::Engine<group_vector_t> integrator;
            const Integrator::AccelerationFunction<group_vector_t> accel_lambda;

            static int validateIndex(int index)
            {
                if (index < 0 || index >= nparticles)
                    throw std::runtime_error("Invalid particle index");

                return index;
            }

            group_vector_t acceleration(const group_vector_t& r, const group_vector_t& v)
            {
                // FIXFIXFIX: add friction/viscosity, possibly using `v`.
                // For now, we calculate energy-conservative forces, divide by
                // particle mass to obtain accelerations. Return all the particle accelerations.
                group_vector_t a;

                // The outermost particles (0 and n-1) are anchors.
                // Their accelerations must always remain 0.
                // Calculate accelerations for:
                // particle[1], connected to [0] and [2].
                // particle[2], connected to [1] and [3].
                // ...
                // particle[n-2], connected to particle[n-3] and particle[n-1].
                // Consecutive particles apply equal and opposite tension forces on each other.

                for (int i = 0; i+1 < nparticles; ++i)
                {
                    PhysicsVector dr = r.array[i+1] - r.array[i];
                    float length = Magnitude(dr);
                    PhysicsVector acc = (stiffness / mass) * (1 - restLength/length) * dr;
                    if (i > 0)
                        a.array[i] += acc;
                    if (i+2 < nparticles)
                        a.array[i+1] -= acc;
                }

                return a;
            }
        };
    }
}
