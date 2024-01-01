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
    };

    class NucleusEngine
    {
    private:
        std::vector<Particle> array;

    public:
        explicit NucleusEngine(std::size_t _nParticles)
            : array(_nParticles)
            {}

        void update()
        {
        }

        std::size_t numParticles() const
        {
            return array.size();
        }

        Particle& particle(int index)
        {
            return array.at(index);
        }

        const Particle& particle(int index) const
        {
            return array.at(index);
        }
    };
}
