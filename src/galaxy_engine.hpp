// Sapphire Galaxy - Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire project:
// https://github.com/cosinekitty/sapphire
//
// This reverb is an adaptation of Airwindows Galactic by Chris Johnson:
// https://github.com/airwindows/airwindows

#pragma once
#include <memory>
#include <cstdint>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    namespace Galaxy
    {
        struct ChannelState
        {
            double iirA;
            double iirB;
            double vibM;
            double feedbackA;
            double feedbackB;
            double feedbackC;
            double feedbackD;
            uint32_t fpd;
            double lastRef[7];
            double thunder;
            double aA[9700];
            double aB[6000];
            double aC[2320];
            double aD[940];
            double aE[15220];
            double aF[8460];
            double aG[4540];
            double aH[3200];
            double aI[6480];
            double aJ[3660];
            double aK[1720];
            double aL[680];
            double aM[3111];
        };


        struct SharedState
        {
            double depthM;
            double vibM;
            double oldfpd;
            int cycle;
            int countA, delayA;
            int countB, delayB;
            int countC, delayC;
            int countD, delayD;
            int countE, delayE;
            int countF, delayF;
            int countG, delayG;
            int countH, delayH;
            int countI, delayI;
            int countJ, delayJ;
            int countK, delayK;
            int countL, delayL;
            int countM, delayM;
        };


        struct ReverbParameters
        {
            float A{};
            float B{};
            float C{};
            float D{};
            float E{};
        };


        struct ReverbState
        {
            ChannelState channel[2];        // 0 = left, 1 = right
            SharedState shared;
            ReverbParameters parms;

            ReverbState()
            {
                clear();
            }

            void clear()
            {
                memset(&channel[0], 0, sizeof(ChannelState));
                memset(&channel[1], 0, sizeof(ChannelState));
                memset(&shared, 0, sizeof(SharedState));
            }

            void process(double inputSampleL, double inputSampleR, double& outputSampleL, double& outputSampleR)
            {
                outputSampleL = inputSampleL;
                outputSampleR = inputSampleR;
            }
        };


        class Engine
        {
        public:
            Engine()
            {
                initialize();
            }

            void initialize()
            {
                clear();
            }

            void clear()
            {
                state->clear();
            }

            void process(float inLeft, float inRight, float& outLeft, float& outRight)
            {
                double resultLeft, resultRight;
                state->process(inLeft, inRight, resultLeft, resultRight);
                outLeft = static_cast<float>(resultLeft);
                outRight = static_cast<float>(resultRight);
            }

        private:
            std::unique_ptr<ReverbState> state = std::make_unique<ReverbState>();
        };
    }
}
