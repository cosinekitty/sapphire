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
        private:
            ChannelState L;
            ChannelState R;
            SharedState  S;

            static inline double dither(double sample, uint32_t& fpd)
            {
                int expon;
                frexpf((float)sample, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                return sample + ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
            }

        public:
            ReverbParameters parm;

            ReverbState()
            {
                clear();
            }

            void clear()
            {
                memset(&L, 0, sizeof(ChannelState));
                memset(&R, 0, sizeof(ChannelState));
                memset(&S, 0, sizeof(SharedState));
            }


            void process(double sampleRateHz, double inputSampleL, double inputSampleR, double& outputSampleL, double& outputSampleR)
            {
                const double overallscale = sampleRateHz / 44100;

                int cycleEnd = std::floor(overallscale);
                if (cycleEnd < 1) cycleEnd = 1;
                if (cycleEnd > 4) cycleEnd = 4;
                //this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
                if (S.cycle > cycleEnd-1) S.cycle = cycleEnd-1; //sanity check

                double regen = 0.0625+((1.0-parm.A)*0.0625);
                double attenuate = (1.0 - (regen / 0.125))*1.333;
                double lowpass = pow(1.00001-(1.0-parm.B),2.0)/std::sqrt(overallscale);
                double drift = std::pow(parm.C,3.0)*0.001;
                double size = (parm.D*1.77)+0.1;
                double wet = 1.0-(pow(1.0-parm.E,3));

                S.delayI = 3407.0*size;
                S.delayJ = 1823.0*size;
                S.delayK = 859.0*size;
                S.delayL = 331.0*size;
                S.delayA = 4801.0*size;
                S.delayB = 2909.0*size;
                S.delayC = 1153.0*size;
                S.delayD = 461.0*size;
                S.delayE = 7607.0*size;
                S.delayF = 4217.0*size;
                S.delayG = 2269.0*size;
                S.delayH = 1597.0*size;
                S.delayM = 256;

                if (std::abs(inputSampleL)<1.18e-23) inputSampleL = L.fpd * 1.18e-17;
                if (std::abs(inputSampleR)<1.18e-23) inputSampleR = R.fpd * 1.18e-17;
                double drySampleL = inputSampleL;
                double drySampleR = inputSampleR;

                S.vibM += (S.oldfpd*drift);
                if (S.vibM > 2*M_PI)
                {
                    S.vibM = 0;
                    S.oldfpd = 0.4294967295+(L.fpd*0.0000000000618);
                }

                L.aM[S.countM] = inputSampleL * attenuate;
                R.aM[S.countM] = inputSampleR * attenuate;
                S.countM++; if (S.countM < 0 || S.countM > S.delayM) S.countM = 0;

                double offsetML = (std::sin(S.vibM)+1.0)*127;
                double offsetMR = (std::sin(S.vibM+M_PI_2)+1.0)*127;
                int workingML = S.countM + offsetML;
                int workingMR = S.countM + offsetMR;
                double interpolML = (L.aM[workingML-((workingML > S.delayM)?S.delayM+1:0)] * (1-(offsetML-std::floor(offsetML))));
                interpolML += (L.aM[workingML+1-((workingML+1 > S.delayM)?S.delayM+1:0)] * ((offsetML-std::floor(offsetML))) );
                double interpolMR = (R.aM[workingMR-((workingMR > S.delayM)?S.delayM+1:0)] * (1-(offsetMR-floor(offsetMR))));
                interpolMR += (R.aM[workingMR+1-((workingMR+1 > S.delayM)?S.delayM+1:0)] * ((offsetMR-floor(offsetMR))) );
                inputSampleL = L.iirA = (L.iirA*(1.0-lowpass))+(interpolML*lowpass);
                inputSampleR = R.iirA = (R.iirA*(1.0-lowpass))+(interpolMR*lowpass);

                if (++S.cycle == cycleEnd)
                {
                    L.aI[S.countI] = inputSampleL + (L.feedbackA * regen);
                    L.aJ[S.countJ] = inputSampleL + (L.feedbackB * regen);
                    L.aK[S.countK] = inputSampleL + (L.feedbackC * regen);
                    L.aL[S.countL] = inputSampleL + (L.feedbackD * regen);
                    R.aI[S.countI] = inputSampleR + (R.feedbackA * regen);
                    R.aJ[S.countJ] = inputSampleR + (R.feedbackB * regen);
                    R.aK[S.countK] = inputSampleR + (R.feedbackC * regen);
                    R.aL[S.countL] = inputSampleR + (R.feedbackD * regen);

                    S.countI++; if (S.countI < 0 || S.countI > S.delayI) S.countI = 0;
                    S.countJ++; if (S.countJ < 0 || S.countJ > S.delayJ) S.countJ = 0;
                    S.countK++; if (S.countK < 0 || S.countK > S.delayK) S.countK = 0;
                    S.countL++; if (S.countL < 0 || S.countL > S.delayL) S.countL = 0;

                    double outIL = L.aI[S.countI-((S.countI > S.delayI)?S.delayI+1:0)];
                    double outJL = L.aJ[S.countJ-((S.countJ > S.delayJ)?S.delayJ+1:0)];
                    double outKL = L.aK[S.countK-((S.countK > S.delayK)?S.delayK+1:0)];
                    double outLL = L.aL[S.countL-((S.countL > S.delayL)?S.delayL+1:0)];
                    double outIR = R.aI[S.countI-((S.countI > S.delayI)?S.delayI+1:0)];
                    double outJR = R.aJ[S.countJ-((S.countJ > S.delayJ)?S.delayJ+1:0)];
                    double outKR = R.aK[S.countK-((S.countK > S.delayK)?S.delayK+1:0)];
                    double outLR = R.aL[S.countL-((S.countL > S.delayL)?S.delayL+1:0)];

                    L.aA[S.countA] = (outIL - (outJL + outKL + outLL));
                    L.aB[S.countB] = (outJL - (outIL + outKL + outLL));
                    L.aC[S.countC] = (outKL - (outIL + outJL + outLL));
                    L.aD[S.countD] = (outLL - (outIL + outJL + outKL));
                    R.aA[S.countA] = (outIR - (outJR + outKR + outLR));
                    R.aB[S.countB] = (outJR - (outIR + outKR + outLR));
                    R.aC[S.countC] = (outKR - (outIR + outJR + outLR));
                    R.aD[S.countD] = (outLR - (outIR + outJR + outKR));

                    S.countA++; if (S.countA < 0 || S.countA > S.delayA) S.countA = 0;
                    S.countB++; if (S.countB < 0 || S.countB > S.delayB) S.countB = 0;
                    S.countC++; if (S.countC < 0 || S.countC > S.delayC) S.countC = 0;
                    S.countD++; if (S.countD < 0 || S.countD > S.delayD) S.countD = 0;

                    double outAL = L.aA[S.countA-((S.countA > S.delayA)?S.delayA+1:0)];
                    double outBL = L.aB[S.countB-((S.countB > S.delayB)?S.delayB+1:0)];
                    double outCL = L.aC[S.countC-((S.countC > S.delayC)?S.delayC+1:0)];
                    double outDL = L.aD[S.countD-((S.countD > S.delayD)?S.delayD+1:0)];
                    double outAR = R.aA[S.countA-((S.countA > S.delayA)?S.delayA+1:0)];
                    double outBR = R.aB[S.countB-((S.countB > S.delayB)?S.delayB+1:0)];
                    double outCR = R.aC[S.countC-((S.countC > S.delayC)?S.delayC+1:0)];
                    double outDR = R.aD[S.countD-((S.countD > S.delayD)?S.delayD+1:0)];

                    L.aE[S.countE] = (outAL - (outBL + outCL + outDL));
                    L.aF[S.countF] = (outBL - (outAL + outCL + outDL));
                    L.aG[S.countG] = (outCL - (outAL + outBL + outDL));
                    L.aH[S.countH] = (outDL - (outAL + outBL + outCL));
                    R.aE[S.countE] = (outAR - (outBR + outCR + outDR));
                    R.aF[S.countF] = (outBR - (outAR + outCR + outDR));
                    R.aG[S.countG] = (outCR - (outAR + outBR + outDR));
                    R.aH[S.countH] = (outDR - (outAR + outBR + outCR));

                    S.countE++; if (S.countE < 0 || S.countE > S.delayE) S.countE = 0;
                    S.countF++; if (S.countF < 0 || S.countF > S.delayF) S.countF = 0;
                    S.countG++; if (S.countG < 0 || S.countG > S.delayG) S.countG = 0;
                    S.countH++; if (S.countH < 0 || S.countH > S.delayH) S.countH = 0;

                    double outEL = L.aE[S.countE-((S.countE > S.delayE)?S.delayE+1:0)];
                    double outFL = L.aF[S.countF-((S.countF > S.delayF)?S.delayF+1:0)];
                    double outGL = L.aG[S.countG-((S.countG > S.delayG)?S.delayG+1:0)];
                    double outHL = L.aH[S.countH-((S.countH > S.delayH)?S.delayH+1:0)];
                    double outER = R.aE[S.countE-((S.countE > S.delayE)?S.delayE+1:0)];
                    double outFR = R.aF[S.countF-((S.countF > S.delayF)?S.delayF+1:0)];
                    double outGR = R.aG[S.countG-((S.countG > S.delayG)?S.delayG+1:0)];
                    double outHR = R.aH[S.countH-((S.countH > S.delayH)?S.delayH+1:0)];

                    L.feedbackA = (outEL - (outFL + outGL + outHL));
                    L.feedbackB = (outFL - (outEL + outGL + outHL));
                    L.feedbackC = (outGL - (outEL + outFL + outHL));
                    L.feedbackD = (outHL - (outEL + outFL + outGL));
                    R.feedbackA = (outER - (outFR + outGR + outHR));
                    R.feedbackB = (outFR - (outER + outGR + outHR));
                    R.feedbackC = (outGR - (outER + outFR + outHR));
                    R.feedbackD = (outHR - (outER + outFR + outGR));

                    inputSampleL = (outEL + outFL + outGL + outHL)/8;
                    inputSampleR = (outER + outFR + outGR + outHR)/8;
                    switch (cycleEnd)
                    {
                    case 4:
                        L.lastRef[0] = L.lastRef[4];
                        L.lastRef[2] = (L.lastRef[0] + inputSampleL)/2;
                        L.lastRef[1] = (L.lastRef[0] + L.lastRef[2])/2;
                        L.lastRef[3] = (L.lastRef[2] + inputSampleL)/2;
                        L.lastRef[4] = inputSampleL;
                        R.lastRef[0] = R.lastRef[4];
                        R.lastRef[2] = (R.lastRef[0] + inputSampleR)/2;
                        R.lastRef[1] = (R.lastRef[0] + R.lastRef[2])/2;
                        R.lastRef[3] = (R.lastRef[2] + inputSampleR)/2;
                        R.lastRef[4] = inputSampleR;
                        break;

                    case 3:
                        L.lastRef[0] = L.lastRef[3];
                        L.lastRef[2] = (L.lastRef[0]+L.lastRef[0]+inputSampleL)/3;
                        L.lastRef[1] = (L.lastRef[0]+inputSampleL+inputSampleL)/3;
                        L.lastRef[3] = inputSampleL;
                        R.lastRef[0] = R.lastRef[3];
                        R.lastRef[2] = (R.lastRef[0]+R.lastRef[0]+inputSampleR)/3;
                        R.lastRef[1] = (R.lastRef[0]+inputSampleR+inputSampleR)/3;
                        R.lastRef[3] = inputSampleR;
                        break;

                    case 2:
                        L.lastRef[0] = L.lastRef[2];
                        L.lastRef[1] = (L.lastRef[0] + inputSampleL)/2;
                        L.lastRef[2] = inputSampleL;
                        R.lastRef[0] = R.lastRef[2];
                        R.lastRef[1] = (R.lastRef[0] + inputSampleR)/2;
                        R.lastRef[2] = inputSampleR;
                        break;

                    case 1:
                        L.lastRef[0] = inputSampleL;
                        R.lastRef[0] = inputSampleR;
                        break;
                    }
                    S.cycle = 0;
                }
                inputSampleL = L.iirB = (L.iirB*(1.0-lowpass))+(L.lastRef[S.cycle]*lowpass);
                inputSampleR = R.iirB = (R.iirB*(1.0-lowpass))+(R.lastRef[S.cycle]*lowpass);
                if (wet < 1.0)
                {
                    inputSampleL = (inputSampleL * wet) + (drySampleL * (1-wet));
                    inputSampleR = (inputSampleR * wet) + (drySampleR * (1-wet));
                }
                outputSampleL = dither(inputSampleL, L.fpd);
                outputSampleR = dither(inputSampleR, R.fpd);
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

            void process(float sampleRate, float inLeft, float inRight, float& outLeft, float& outRight)
            {
                double resultLeft, resultRight;
                state->process(sampleRate, inLeft, inRight, resultLeft, resultRight);
                outLeft = static_cast<float>(resultLeft);
                outRight = static_cast<float>(resultRight);
            }

            ReverbParameters& parameters()
            {
                return state->parm;
            }

        private:
            std::unique_ptr<ReverbState> state = std::make_unique<ReverbState>();
        };
    }
}
