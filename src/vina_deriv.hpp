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

                y[0].pos = 0;
                y[0].vel = 0;

                y[1].pos = x[1].vel;
                acc = k*((x[1].pos - x[0].pos) - restLength);
                y[1].vel = -acc;

                y[2].pos = x[2].vel;
                acc = k*((x[2].pos - x[1].pos) - restLength);
                y[1].vel += acc;
                y[2].vel = -acc;

                y[3].pos = x[3].vel;
                acc = k*((x[3].pos - x[2].pos) - restLength);
                y[2].vel += acc;
                y[3].vel = -acc;

                y[4].pos = x[4].vel;
                acc = k*((x[4].pos - x[3].pos) - restLength);
                y[3].vel += acc;
                y[4].vel = -acc;

                y[5].pos = x[5].vel;
                acc = k*((x[5].pos - x[4].pos) - restLength);
                y[4].vel += acc;
                y[5].vel = -acc;

                y[6].pos = x[6].vel;
                acc = k*((x[6].pos - x[5].pos) - restLength);
                y[5].vel += acc;
                y[6].vel = -acc;

                y[7].pos = x[7].vel;
                acc = k*((x[7].pos - x[6].pos) - restLength);
                y[6].vel += acc;
                y[7].vel = -acc;

                y[8].pos = x[8].vel;
                acc = k*((x[8].pos - x[7].pos) - restLength);
                y[7].vel += acc;
                y[8].vel = -acc;

                y[9].pos = x[9].vel;
                acc = k*((x[9].pos - x[8].pos) - restLength);
                y[8].vel += acc;
                y[9].vel = -acc;

                y[10].pos = x[10].vel;
                acc = k*((x[10].pos - x[9].pos) - restLength);
                y[9].vel += acc;
                y[10].vel = -acc;

                y[11].pos = x[11].vel;
                acc = k*((x[11].pos - x[10].pos) - restLength);
                y[10].vel += acc;
                y[11].vel = -acc;

                y[12].pos = x[12].vel;
                acc = k*((x[12].pos - x[11].pos) - restLength);
                y[11].vel += acc;
                y[12].vel = -acc;

                y[13].pos = x[13].vel;
                acc = k*((x[13].pos - x[12].pos) - restLength);
                y[12].vel += acc;
                y[13].vel = -acc;

                y[14].pos = x[14].vel;
                acc = k*((x[14].pos - x[13].pos) - restLength);
                y[13].vel += acc;
                y[14].vel = -acc;

                y[15].pos = x[15].vel;
                acc = k*((x[15].pos - x[14].pos) - restLength);
                y[14].vel += acc;
                y[15].vel = -acc;

                y[16].pos = x[16].vel;
                acc = k*((x[16].pos - x[15].pos) - restLength);
                y[15].vel += acc;
                y[16].vel = -acc;

                y[17].pos = x[17].vel;
                acc = k*((x[17].pos - x[16].pos) - restLength);
                y[16].vel += acc;
                y[17].vel = -acc;

                y[18].pos = x[18].vel;
                acc = k*((x[18].pos - x[17].pos) - restLength);
                y[17].vel += acc;
                y[18].vel = -acc;

                y[19].pos = x[19].vel;
                acc = k*((x[19].pos - x[18].pos) - restLength);
                y[18].vel += acc;
                y[19].vel = -acc;

                y[20].pos = x[20].vel;
                acc = k*((x[20].pos - x[19].pos) - restLength);
                y[19].vel += acc;
                y[20].vel = -acc;

                y[21].pos = x[21].vel;
                acc = k*((x[21].pos - x[20].pos) - restLength);
                y[20].vel += acc;
                y[21].vel = -acc;

                y[22].pos = x[22].vel;
                acc = k*((x[22].pos - x[21].pos) - restLength);
                y[21].vel += acc;
                y[22].vel = -acc;

                y[23].pos = x[23].vel;
                acc = k*((x[23].pos - x[22].pos) - restLength);
                y[22].vel += acc;
                y[23].vel = -acc;

                y[24].pos = x[24].vel;
                acc = k*((x[24].pos - x[23].pos) - restLength);
                y[23].vel += acc;
                y[24].vel = -acc;

                y[25].pos = x[25].vel;
                acc = k*((x[25].pos - x[24].pos) - restLength);
                y[24].vel += acc;
                y[25].vel = -acc;

                y[26].pos = x[26].vel;
                acc = k*((x[26].pos - x[25].pos) - restLength);
                y[25].vel += acc;
                y[26].vel = -acc;

                y[27].pos = x[27].vel;
                acc = k*((x[27].pos - x[26].pos) - restLength);
                y[26].vel += acc;
                y[27].vel = -acc;

                y[28].pos = x[28].vel;
                acc = k*((x[28].pos - x[27].pos) - restLength);
                y[27].vel += acc;
                y[28].vel = -acc;

                y[29].pos = x[29].vel;
                acc = k*((x[29].pos - x[28].pos) - restLength);
                y[28].vel += acc;
                y[29].vel = -acc;

                y[30].pos = x[30].vel;
                acc = k*((x[30].pos - x[29].pos) - restLength);
                y[29].vel += acc;
                y[30].vel = -acc;

                y[31].pos = x[31].vel;
                acc = k*((x[31].pos - x[30].pos) - restLength);
                y[30].vel += acc;
                y[31].vel = -acc;

                y[32].pos = x[32].vel;
                acc = k*((x[32].pos - x[31].pos) - restLength);
                y[31].vel += acc;
                y[32].vel = -acc;

                y[33].pos = x[33].vel;
                acc = k*((x[33].pos - x[32].pos) - restLength);
                y[32].vel += acc;
                y[33].vel = -acc;

                y[34].pos = x[34].vel;
                acc = k*((x[34].pos - x[33].pos) - restLength);
                y[33].vel += acc;
                y[34].vel = -acc;

                y[35].pos = x[35].vel;
                acc = k*((x[35].pos - x[34].pos) - restLength);
                y[34].vel += acc;
                y[35].vel = -acc;

                y[36].pos = x[36].vel;
                acc = k*((x[36].pos - x[35].pos) - restLength);
                y[35].vel += acc;
                y[36].vel = -acc;

                y[37].pos = x[37].vel;
                acc = k*((x[37].pos - x[36].pos) - restLength);
                y[36].vel += acc;
                y[37].vel = -acc;

                y[38].pos = x[38].vel;
                acc = k*((x[38].pos - x[37].pos) - restLength);
                y[37].vel += acc;
                y[38].vel = -acc;

                y[39].pos = x[39].vel;
                acc = k*((x[39].pos - x[38].pos) - restLength);
                y[38].vel += acc;
                y[39].vel = -acc;

                y[40].pos = x[40].vel;
                acc = k*((x[40].pos - x[39].pos) - restLength);
                y[39].vel += acc;
                y[40].vel = -acc;

                y[41].pos = x[41].vel;
                acc = k*((x[41].pos - x[40].pos) - restLength);
                y[40].vel += acc;
                y[41].vel = -acc;

                y[42].pos = x[42].vel;
                acc = k*((x[42].pos - x[41].pos) - restLength);
                y[41].vel += acc;
                y[42].vel = -acc;

                y[43].pos = 0;
                acc = k*((x[43].pos - x[42].pos) - restLength);
                y[42].vel += acc;
                y[43].vel = 0;
            }
        };
    }
}
