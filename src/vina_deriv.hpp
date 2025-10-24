//**** GENERATED CODE **** DO NOT EDIT ****
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        struct VinaDeriv
        {
            float k = 1000 * defaultStiffness;            // stiffness/mass, where mass = 0.001 kg
            float restLength = 0.004;

            explicit VinaDeriv() {}

            void operator() (vina_state_t& y, const vina_state_t& x)
            {
                float acc;

                y[0].pos = x[0].vel;
                y[1].pos = x[1].vel;
                y[2].pos = x[2].vel;
                y[3].pos = x[3].vel;
                y[4].pos = x[4].vel;
                y[5].pos = x[5].vel;
                y[6].pos = x[6].vel;
                y[7].pos = x[7].vel;
                y[8].pos = x[8].vel;
                y[9].pos = x[9].vel;
                y[10].pos = x[10].vel;

                y[0].vel[0] = 0;

                acc = k*((x[0].pos[1] - x[0].pos[0]) - restLength);
                y[0].vel[1] = -acc;

                acc = k*((x[0].pos[2] - x[0].pos[1]) - restLength);
                y[0].vel[1] += acc;
                y[0].vel[2] = -acc;

                acc = k*((x[0].pos[3] - x[0].pos[2]) - restLength);
                y[0].vel[2] += acc;
                y[0].vel[3] = -acc;

                acc = k*((x[1].pos[0] - x[0].pos[3]) - restLength);
                y[0].vel[3] += acc;
                y[1].vel[0] = -acc;

                acc = k*((x[1].pos[1] - x[1].pos[0]) - restLength);
                y[1].vel[0] += acc;
                y[1].vel[1] = -acc;

                acc = k*((x[1].pos[2] - x[1].pos[1]) - restLength);
                y[1].vel[1] += acc;
                y[1].vel[2] = -acc;

                acc = k*((x[1].pos[3] - x[1].pos[2]) - restLength);
                y[1].vel[2] += acc;
                y[1].vel[3] = -acc;

                acc = k*((x[2].pos[0] - x[1].pos[3]) - restLength);
                y[1].vel[3] += acc;
                y[2].vel[0] = -acc;

                acc = k*((x[2].pos[1] - x[2].pos[0]) - restLength);
                y[2].vel[0] += acc;
                y[2].vel[1] = -acc;

                acc = k*((x[2].pos[2] - x[2].pos[1]) - restLength);
                y[2].vel[1] += acc;
                y[2].vel[2] = -acc;

                acc = k*((x[2].pos[3] - x[2].pos[2]) - restLength);
                y[2].vel[2] += acc;
                y[2].vel[3] = -acc;

                acc = k*((x[3].pos[0] - x[2].pos[3]) - restLength);
                y[2].vel[3] += acc;
                y[3].vel[0] = -acc;

                acc = k*((x[3].pos[1] - x[3].pos[0]) - restLength);
                y[3].vel[0] += acc;
                y[3].vel[1] = -acc;

                acc = k*((x[3].pos[2] - x[3].pos[1]) - restLength);
                y[3].vel[1] += acc;
                y[3].vel[2] = -acc;

                acc = k*((x[3].pos[3] - x[3].pos[2]) - restLength);
                y[3].vel[2] += acc;
                y[3].vel[3] = -acc;

                acc = k*((x[4].pos[0] - x[3].pos[3]) - restLength);
                y[3].vel[3] += acc;
                y[4].vel[0] = -acc;

                acc = k*((x[4].pos[1] - x[4].pos[0]) - restLength);
                y[4].vel[0] += acc;
                y[4].vel[1] = -acc;

                acc = k*((x[4].pos[2] - x[4].pos[1]) - restLength);
                y[4].vel[1] += acc;
                y[4].vel[2] = -acc;

                acc = k*((x[4].pos[3] - x[4].pos[2]) - restLength);
                y[4].vel[2] += acc;
                y[4].vel[3] = -acc;

                acc = k*((x[5].pos[0] - x[4].pos[3]) - restLength);
                y[4].vel[3] += acc;
                y[5].vel[0] = -acc;

                acc = k*((x[5].pos[1] - x[5].pos[0]) - restLength);
                y[5].vel[0] += acc;
                y[5].vel[1] = -acc;

                acc = k*((x[5].pos[2] - x[5].pos[1]) - restLength);
                y[5].vel[1] += acc;
                y[5].vel[2] = -acc;

                acc = k*((x[5].pos[3] - x[5].pos[2]) - restLength);
                y[5].vel[2] += acc;
                y[5].vel[3] = -acc;

                acc = k*((x[6].pos[0] - x[5].pos[3]) - restLength);
                y[5].vel[3] += acc;
                y[6].vel[0] = -acc;

                acc = k*((x[6].pos[1] - x[6].pos[0]) - restLength);
                y[6].vel[0] += acc;
                y[6].vel[1] = -acc;

                acc = k*((x[6].pos[2] - x[6].pos[1]) - restLength);
                y[6].vel[1] += acc;
                y[6].vel[2] = -acc;

                acc = k*((x[6].pos[3] - x[6].pos[2]) - restLength);
                y[6].vel[2] += acc;
                y[6].vel[3] = -acc;

                acc = k*((x[7].pos[0] - x[6].pos[3]) - restLength);
                y[6].vel[3] += acc;
                y[7].vel[0] = -acc;

                acc = k*((x[7].pos[1] - x[7].pos[0]) - restLength);
                y[7].vel[0] += acc;
                y[7].vel[1] = -acc;

                acc = k*((x[7].pos[2] - x[7].pos[1]) - restLength);
                y[7].vel[1] += acc;
                y[7].vel[2] = -acc;

                acc = k*((x[7].pos[3] - x[7].pos[2]) - restLength);
                y[7].vel[2] += acc;
                y[7].vel[3] = -acc;

                acc = k*((x[8].pos[0] - x[7].pos[3]) - restLength);
                y[7].vel[3] += acc;
                y[8].vel[0] = -acc;

                acc = k*((x[8].pos[1] - x[8].pos[0]) - restLength);
                y[8].vel[0] += acc;
                y[8].vel[1] = -acc;

                acc = k*((x[8].pos[2] - x[8].pos[1]) - restLength);
                y[8].vel[1] += acc;
                y[8].vel[2] = -acc;

                acc = k*((x[8].pos[3] - x[8].pos[2]) - restLength);
                y[8].vel[2] += acc;
                y[8].vel[3] = -acc;

                acc = k*((x[9].pos[0] - x[8].pos[3]) - restLength);
                y[8].vel[3] += acc;
                y[9].vel[0] = -acc;

                acc = k*((x[9].pos[1] - x[9].pos[0]) - restLength);
                y[9].vel[0] += acc;
                y[9].vel[1] = -acc;

                acc = k*((x[9].pos[2] - x[9].pos[1]) - restLength);
                y[9].vel[1] += acc;
                y[9].vel[2] = -acc;

                acc = k*((x[9].pos[3] - x[9].pos[2]) - restLength);
                y[9].vel[2] += acc;
                y[9].vel[3] = -acc;

                acc = k*((x[10].pos[0] - x[9].pos[3]) - restLength);
                y[9].vel[3] += acc;
                y[10].vel[0] = -acc;

                acc = k*((x[10].pos[1] - x[10].pos[0]) - restLength);
                y[10].vel[0] += acc;
                y[10].vel[1] = -acc;

                acc = k*((x[10].pos[2] - x[10].pos[1]) - restLength);
                y[10].vel[1] += acc;
                y[10].vel[2] = -acc;

                acc = k*((x[10].pos[3] - x[10].pos[2]) - restLength);
                y[10].vel[2] += acc;
                y[10].vel[3] = 0;
            }
        };
    }
}
