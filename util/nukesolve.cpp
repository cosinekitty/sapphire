/*
    nukesolve.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#include <random>
#include "nucleus_engine.hpp"

int main()
{
    using namespace Sapphire;

    const std::size_t NUM_PARTICLES = 5;
    NucleusEngine engine{NUM_PARTICLES};

    std::mt19937 rand{0x7c3aaf29u};     // seed was chosen using rolls of physical hexadecimal dice

    const double mean = 0.0;
    const double dev = 1.0;
    std::normal_distribution norm{mean, dev};

    // The first particle, P[0], will be held fixed at the origin.
    // for now, skip it and initialize 1..(N-1) using random positions.
    // Velocities will all start at zero and strong friction will be used
    // to overdamp toward convergence.

    for (std::size_t i = 1; i < NUM_PARTICLES; ++i)
    {
        Particle& p = engine.particle(i);
        p.pos[0] = norm(rand);
        p.pos[1] = norm(rand);
        p.pos[2] = norm(rand);
        p.pos[3] = 0.0f;

        p.vel = PhysicsVector::zero();
    }

    // FIXFIXFIX - Run the simulation until convergence.
    Particle &input = engine.particle(0);
    input.pos = input.vel = PhysicsVector::zero();
    engine.update();

    // Output the solved state.
    // FIXFIXFIX - Write to a C++ header file.

    printf("n=%u\n", static_cast<unsigned>(NUM_PARTICLES));
    for (std::size_t i = 0; i < NUM_PARTICLES; ++i)
    {
        Particle& p = engine.particle(i);
        printf("i=%u rx=%0.16lg ry=%0.16lg rz=%0.16lg vx=%0.16lg vy=%0.16lg vz=%0.16lg\n",
            static_cast<unsigned>(i),
            p.pos[0], p.pos[1], p.pos[2],
            p.vel[0], p.vel[1], p.vel[2]
        );
    }

    return 0;
}
