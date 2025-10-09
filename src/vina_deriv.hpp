//**** GENERATED CODE **** DO NOT EDIT ****
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        struct VinaDeriv
        {
            PhysicsVector gravity;
            float stiffness = 89.0;
            float restLength = 0.004;
            float mass = 1.0e-03;

            explicit VinaDeriv() {}

            void operator() (vina_state_t& slope, const vina_state_t& state)
            {
                assert(nParticles == slope.size());
                assert(nParticles == state.size());
                assert(nParticles > 2);

                // Derivative of left anchor.
                slope[0].pos = state[0].vel;
                slope[0].vel = PhysicsVector{};

                // Derivatives of the mobile particles.
                for (unsigned i = 1; i < nParticles; ++i)
                {
                    slope[i].pos = state[i].vel;
                    slope[i].vel = (i < nParticles-1) ? gravity : PhysicsVector{};
                    const PhysicsVector dr = state[i].pos - state[i-1].pos;
                    const double length = dr.mag();
                    const double fmag = stiffness*(length - restLength);
                    const PhysicsVector acc = (fmag/(mass * length)) * dr;
                    if (i >= 2)
                        slope[i-1].vel += acc;
                    if (i < nParticles-1)
                        slope[i].vel -= acc;
                }
            }
        };
    }
}
