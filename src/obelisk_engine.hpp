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
                for (int i = 0; i < nparticles; ++i)
                    array[i] = init;
            }
        };


        template <int nparticles>
        class Engine
        {
        private:
            static_assert(nparticles >= 3, "The number of particles must be 3 or greater.");
            Integrator::Engine<GroupVector<nparticles>> integrator;

            static int validateIndex(int index)
            {
                if (index < 0 || index >= nparticles)
                    throw std::runtime_error("Invalid particle index");

                return index;
            }

        public:
            using state_vector_t = Integrator::StateVector<GroupVector<nparticles>>;

            Engine()
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

            }
        };
    }
}
