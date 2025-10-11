//**** GENERATED CODE **** DO NOT EDIT ****
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        struct VinaDeriv
        {
            float stiffness = 89.0;
            float restLength = 0.004;
            float mass = 1.0e-03;

            explicit VinaDeriv() {}

            void operator() (vina_state_t& slope, const vina_state_t& state)
            {
                assert(nParticles == slope.size());
                assert(nParticles == state.size());
                assert(nParticles > 2);

                slope[0].pos = state[0].vel;
                slope[0].vel = 0;
                for (unsigned i = 1; i < nParticles; ++i)
                {
                    slope[i].pos = state[i].vel;
                    slope[i].vel = 0;
                    float dr = state[i].pos - state[i-1].pos;
                    float length = std::abs(dr);
                    float fmag = stiffness*(length - restLength);
                    float acc = (fmag/(mass * length)) * dr;
                    if (i >= 2)
                        slope[i-1].vel += acc;
                    if (i < nParticles-1)
                        slope[i].vel -= acc;
                }
            }
        };
    }
}
