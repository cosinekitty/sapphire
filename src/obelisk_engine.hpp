/*
    obelisk_engine.hpp   -   Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once
#include <vector>
#include "sapphire_simd.hpp"

namespace Sapphire
{
    namespace Obelisk
    {
        const int MinParticleCount = 3;
        const float InitialParticleSpacingMeters = 1.0e-3;
        const float InitialParticleMass = 1.0e-3;

        class Engine
        {
        private:
            std::vector<Particle> array;

        public:
            explicit Engine(int _nparticles)
            {
                const std::size_t n = static_cast<std::size_t>(std::max(MinParticleCount, _nparticles));
                array.resize(n);
                initialize();
            }

            void initialize()
            {
                const int n = array.size();
                for (int i = 0; i < n; ++i)
                {
                    array[i].pos = PhysicsVector(i*InitialParticleSpacingMeters, 0, 0, 0);
                    array[i].vel = PhysicsVector::zero();
                    array[i].force = PhysicsVector::zero();
                    array[i].mass = (i>0 && i+1<n) ? InitialParticleMass : AnchorMass;
                }
            }
        };
    }
}
