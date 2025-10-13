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
                float dr, length, fmag, acc;

                // i = 0
                slope[0].pos = state[0].vel;
                slope[0].vel = 0;

                // i = 1
                slope[1].pos = state[1].vel;
                dr = state[1].pos - state[0].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[1].vel = -acc;

                // i = 2
                slope[2].pos = state[2].vel;
                dr = state[2].pos - state[1].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[1].vel += acc;
                slope[2].vel = -acc;

                // i = 3
                slope[3].pos = state[3].vel;
                dr = state[3].pos - state[2].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[2].vel += acc;
                slope[3].vel = -acc;

                // i = 4
                slope[4].pos = state[4].vel;
                dr = state[4].pos - state[3].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[3].vel += acc;
                slope[4].vel = -acc;

                // i = 5
                slope[5].pos = state[5].vel;
                dr = state[5].pos - state[4].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[4].vel += acc;
                slope[5].vel = -acc;

                // i = 6
                slope[6].pos = state[6].vel;
                dr = state[6].pos - state[5].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[5].vel += acc;
                slope[6].vel = -acc;

                // i = 7
                slope[7].pos = state[7].vel;
                dr = state[7].pos - state[6].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[6].vel += acc;
                slope[7].vel = -acc;

                // i = 8
                slope[8].pos = state[8].vel;
                dr = state[8].pos - state[7].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[7].vel += acc;
                slope[8].vel = -acc;

                // i = 9
                slope[9].pos = state[9].vel;
                dr = state[9].pos - state[8].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[8].vel += acc;
                slope[9].vel = -acc;

                // i = 10
                slope[10].pos = state[10].vel;
                dr = state[10].pos - state[9].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[9].vel += acc;
                slope[10].vel = -acc;

                // i = 11
                slope[11].pos = state[11].vel;
                dr = state[11].pos - state[10].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[10].vel += acc;
                slope[11].vel = -acc;

                // i = 12
                slope[12].pos = state[12].vel;
                dr = state[12].pos - state[11].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[11].vel += acc;
                slope[12].vel = -acc;

                // i = 13
                slope[13].pos = state[13].vel;
                dr = state[13].pos - state[12].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[12].vel += acc;
                slope[13].vel = -acc;

                // i = 14
                slope[14].pos = state[14].vel;
                dr = state[14].pos - state[13].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[13].vel += acc;
                slope[14].vel = -acc;

                // i = 15
                slope[15].pos = state[15].vel;
                dr = state[15].pos - state[14].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[14].vel += acc;
                slope[15].vel = -acc;

                // i = 16
                slope[16].pos = state[16].vel;
                dr = state[16].pos - state[15].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[15].vel += acc;
                slope[16].vel = -acc;

                // i = 17
                slope[17].pos = state[17].vel;
                dr = state[17].pos - state[16].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[16].vel += acc;
                slope[17].vel = -acc;

                // i = 18
                slope[18].pos = state[18].vel;
                dr = state[18].pos - state[17].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[17].vel += acc;
                slope[18].vel = -acc;

                // i = 19
                slope[19].pos = state[19].vel;
                dr = state[19].pos - state[18].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[18].vel += acc;
                slope[19].vel = -acc;

                // i = 20
                slope[20].pos = state[20].vel;
                dr = state[20].pos - state[19].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[19].vel += acc;
                slope[20].vel = -acc;

                // i = 21
                slope[21].pos = state[21].vel;
                dr = state[21].pos - state[20].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[20].vel += acc;
                slope[21].vel = -acc;

                // i = 22
                slope[22].pos = state[22].vel;
                dr = state[22].pos - state[21].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[21].vel += acc;
                slope[22].vel = -acc;

                // i = 23
                slope[23].pos = state[23].vel;
                dr = state[23].pos - state[22].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[22].vel += acc;
                slope[23].vel = -acc;

                // i = 24
                slope[24].pos = state[24].vel;
                dr = state[24].pos - state[23].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[23].vel += acc;
                slope[24].vel = -acc;

                // i = 25
                slope[25].pos = state[25].vel;
                dr = state[25].pos - state[24].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[24].vel += acc;
                slope[25].vel = -acc;

                // i = 26
                slope[26].pos = state[26].vel;
                dr = state[26].pos - state[25].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[25].vel += acc;
                slope[26].vel = -acc;

                // i = 27
                slope[27].pos = state[27].vel;
                dr = state[27].pos - state[26].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[26].vel += acc;
                slope[27].vel = -acc;

                // i = 28
                slope[28].pos = state[28].vel;
                dr = state[28].pos - state[27].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[27].vel += acc;
                slope[28].vel = -acc;

                // i = 29
                slope[29].pos = state[29].vel;
                dr = state[29].pos - state[28].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[28].vel += acc;
                slope[29].vel = -acc;

                // i = 30
                slope[30].pos = state[30].vel;
                dr = state[30].pos - state[29].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[29].vel += acc;
                slope[30].vel = -acc;

                // i = 31
                slope[31].pos = state[31].vel;
                dr = state[31].pos - state[30].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[30].vel += acc;
                slope[31].vel = -acc;

                // i = 32
                slope[32].pos = state[32].vel;
                dr = state[32].pos - state[31].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[31].vel += acc;
                slope[32].vel = -acc;

                // i = 33
                slope[33].pos = state[33].vel;
                dr = state[33].pos - state[32].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[32].vel += acc;
                slope[33].vel = -acc;

                // i = 34
                slope[34].pos = state[34].vel;
                dr = state[34].pos - state[33].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[33].vel += acc;
                slope[34].vel = -acc;

                // i = 35
                slope[35].pos = state[35].vel;
                dr = state[35].pos - state[34].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[34].vel += acc;
                slope[35].vel = -acc;

                // i = 36
                slope[36].pos = state[36].vel;
                dr = state[36].pos - state[35].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[35].vel += acc;
                slope[36].vel = -acc;

                // i = 37
                slope[37].pos = state[37].vel;
                dr = state[37].pos - state[36].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[36].vel += acc;
                slope[37].vel = -acc;

                // i = 38
                slope[38].pos = state[38].vel;
                dr = state[38].pos - state[37].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[37].vel += acc;
                slope[38].vel = -acc;

                // i = 39
                slope[39].pos = state[39].vel;
                dr = state[39].pos - state[38].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[38].vel += acc;
                slope[39].vel = -acc;

                // i = 40
                slope[40].pos = state[40].vel;
                dr = state[40].pos - state[39].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[39].vel += acc;
                slope[40].vel = -acc;

                // i = 41
                slope[41].pos = state[41].vel;
                dr = state[41].pos - state[40].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[40].vel += acc;
                slope[41].vel = -acc;

                // i = 42
                slope[42].pos = state[42].vel;
                dr = state[42].pos - state[41].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[41].vel += acc;
                slope[42].vel = -acc;

                // i = 43
                slope[43].pos = state[43].vel;
                dr = state[43].pos - state[42].pos;
                length = (dr < 0) ? -dr : dr;
                fmag = stiffness*(length - restLength);
                acc = (fmag/(mass * length)) * dr;
                slope[42].vel += acc;
                slope[43].vel = 0;

            }
        };
    }
}
