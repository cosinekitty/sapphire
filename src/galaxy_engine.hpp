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
            ChannelState channel[2];        // 0 = left, 1 = right
            SharedState shared;
            ReverbParameters parm;

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

            void process(double sampleRateHz, double inputSampleL, double inputSampleR, double& outputSampleL, double& outputSampleR)
            {
                const double overallscale = sampleRateHz / 44100;

                int cycleEnd = std::floor(overallscale);
                if (cycleEnd < 1) cycleEnd = 1;
                if (cycleEnd > 4) cycleEnd = 4;
                //this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
                if (shared.cycle > cycleEnd-1) shared.cycle = cycleEnd-1; //sanity check

                double regen = 0.0625+((1.0-parm.A)*0.0625);
                double attenuate = (1.0 - (regen / 0.125))*1.333;
                double lowpass = pow(1.00001-(1.0-parm.B),2.0)/std::sqrt(overallscale);
                double drift = std::pow(parm.C,3.0)*0.001;
                double size = (parm.D*1.77)+0.1;
                double wet = 1.0-(pow(1.0-parm.E,3));

                shared.delayI = 3407.0*size;
                shared.delayJ = 1823.0*size;
                shared.delayK = 859.0*size;
                shared.delayL = 331.0*size;
                shared.delayA = 4801.0*size;
                shared.delayB = 2909.0*size;
                shared.delayC = 1153.0*size;
                shared.delayD = 461.0*size;
                shared.delayE = 7607.0*size;
                shared.delayF = 4217.0*size;
                shared.delayG = 2269.0*size;
                shared.delayH = 1597.0*size;
                shared.delayM = 256;

                if (std::abs(inputSampleL)<1.18e-23) inputSampleL = channel[0].fpd * 1.18e-17;
                if (std::abs(inputSampleR)<1.18e-23) inputSampleR = channel[1].fpd * 1.18e-17;
                double drySampleL = inputSampleL;
                double drySampleR = inputSampleR;

                shared.vibM += (shared.oldfpd*drift);
                if (shared.vibM > 2*M_PI)
                {
                    shared.vibM = 0;
                    shared.oldfpd = 0.4294967295+(channel[0].fpd*0.0000000000618);
                }

                channel[0].aM[shared.countM] = inputSampleL * attenuate;
                channel[1].aM[shared.countM] = inputSampleR * attenuate;
                shared.countM++; if (shared.countM < 0 || shared.countM > shared.delayM) shared.countM = 0;

                double offsetML = (std::sin(shared.vibM)+1.0)*127;
                double offsetMR = (std::sin(shared.vibM+M_PI_2)+1.0)*127;
                int workingML = shared.countM + offsetML;
                int workingMR = shared.countM + offsetMR;
                double interpolML = (channel[0].aM[workingML-((workingML > shared.delayM)?shared.delayM+1:0)] * (1-(offsetML-std::floor(offsetML))));
                interpolML += (channel[0].aM[workingML+1-((workingML+1 > shared.delayM)?shared.delayM+1:0)] * ((offsetML-std::floor(offsetML))) );
                double interpolMR = (channel[1].aM[workingMR-((workingMR > shared.delayM)?shared.delayM+1:0)] * (1-(offsetMR-floor(offsetMR))));
                interpolMR += (channel[1].aM[workingMR+1-((workingMR+1 > shared.delayM)?shared.delayM+1:0)] * ((offsetMR-floor(offsetMR))) );
                inputSampleL = interpolML;
                inputSampleR = interpolMR;
                //predelay that applies vibrato
                //want vibrato speed AND depth like in MatrixVerb

                inputSampleL = channel[0].iirA = (channel[0].iirA*(1.0-lowpass))+(inputSampleL*lowpass);
                inputSampleR = channel[1].iirA = (channel[1].iirA*(1.0-lowpass))+(inputSampleR*lowpass);
                //initial filter

                if (++shared.cycle == cycleEnd)
                {
                    channel[0].aI[shared.countI] = inputSampleL + (channel[0].feedbackA * regen);
                    channel[0].aJ[shared.countJ] = inputSampleL + (channel[0].feedbackB * regen);
                    channel[0].aK[shared.countK] = inputSampleL + (channel[0].feedbackC * regen);
                    channel[0].aL[shared.countL] = inputSampleL + (channel[0].feedbackD * regen);
                    channel[1].aI[shared.countI] = inputSampleR + (channel[1].feedbackA * regen);
                    channel[1].aJ[shared.countJ] = inputSampleR + (channel[1].feedbackB * regen);
                    channel[1].aK[shared.countK] = inputSampleR + (channel[1].feedbackC * regen);
                    channel[1].aL[shared.countL] = inputSampleR + (channel[1].feedbackD * regen);

                    shared.countI++; if (shared.countI < 0 || shared.countI > shared.delayI) shared.countI = 0;
                    shared.countJ++; if (shared.countJ < 0 || shared.countJ > shared.delayJ) shared.countJ = 0;
                    shared.countK++; if (shared.countK < 0 || shared.countK > shared.delayK) shared.countK = 0;
                    shared.countL++; if (shared.countL < 0 || shared.countL > shared.delayL) shared.countL = 0;

                    double outIL = channel[0].aI[shared.countI-((shared.countI > shared.delayI)?shared.delayI+1:0)];
                    double outJL = channel[0].aJ[shared.countJ-((shared.countJ > shared.delayJ)?shared.delayJ+1:0)];
                    double outKL = channel[0].aK[shared.countK-((shared.countK > shared.delayK)?shared.delayK+1:0)];
                    double outLL = channel[0].aL[shared.countL-((shared.countL > shared.delayL)?shared.delayL+1:0)];
                    double outIR = channel[1].aI[shared.countI-((shared.countI > shared.delayI)?shared.delayI+1:0)];
                    double outJR = channel[1].aJ[shared.countJ-((shared.countJ > shared.delayJ)?shared.delayJ+1:0)];
                    double outKR = channel[1].aK[shared.countK-((shared.countK > shared.delayK)?shared.delayK+1:0)];
                    double outLR = channel[1].aL[shared.countL-((shared.countL > shared.delayL)?shared.delayL+1:0)];

                    channel[0].aA[shared.countA] = (outIL - (outJL + outKL + outLL));
                    channel[0].aB[shared.countB] = (outJL - (outIL + outKL + outLL));
                    channel[0].aC[shared.countC] = (outKL - (outIL + outJL + outLL));
                    channel[0].aD[shared.countD] = (outLL - (outIL + outJL + outKL));
                    channel[1].aA[shared.countA] = (outIR - (outJR + outKR + outLR));
                    channel[1].aB[shared.countB] = (outJR - (outIR + outKR + outLR));
                    channel[1].aC[shared.countC] = (outKR - (outIR + outJR + outLR));
                    channel[1].aD[shared.countD] = (outLR - (outIR + outJR + outKR));

                    shared.countA++; if (shared.countA < 0 || shared.countA > shared.delayA) shared.countA = 0;
                    shared.countB++; if (shared.countB < 0 || shared.countB > shared.delayB) shared.countB = 0;
                    shared.countC++; if (shared.countC < 0 || shared.countC > shared.delayC) shared.countC = 0;
                    shared.countD++; if (shared.countD < 0 || shared.countD > shared.delayD) shared.countD = 0;

                    double outAL = channel[0].aA[shared.countA-((shared.countA > shared.delayA)?shared.delayA+1:0)];
                    double outBL = channel[0].aB[shared.countB-((shared.countB > shared.delayB)?shared.delayB+1:0)];
                    double outCL = channel[0].aC[shared.countC-((shared.countC > shared.delayC)?shared.delayC+1:0)];
                    double outDL = channel[0].aD[shared.countD-((shared.countD > shared.delayD)?shared.delayD+1:0)];
                    double outAR = channel[1].aA[shared.countA-((shared.countA > shared.delayA)?shared.delayA+1:0)];
                    double outBR = channel[1].aB[shared.countB-((shared.countB > shared.delayB)?shared.delayB+1:0)];
                    double outCR = channel[1].aC[shared.countC-((shared.countC > shared.delayC)?shared.delayC+1:0)];
                    double outDR = channel[1].aD[shared.countD-((shared.countD > shared.delayD)?shared.delayD+1:0)];

                    channel[0].aE[shared.countE] = (outAL - (outBL + outCL + outDL));
                    channel[0].aF[shared.countF] = (outBL - (outAL + outCL + outDL));
                    channel[0].aG[shared.countG] = (outCL - (outAL + outBL + outDL));
                    channel[0].aH[shared.countH] = (outDL - (outAL + outBL + outCL));
                    channel[1].aE[shared.countE] = (outAR - (outBR + outCR + outDR));
                    channel[1].aF[shared.countF] = (outBR - (outAR + outCR + outDR));
                    channel[1].aG[shared.countG] = (outCR - (outAR + outBR + outDR));
                    channel[1].aH[shared.countH] = (outDR - (outAR + outBR + outCR));

                    shared.countE++; if (shared.countE < 0 || shared.countE > shared.delayE) shared.countE = 0;
                    shared.countF++; if (shared.countF < 0 || shared.countF > shared.delayF) shared.countF = 0;
                    shared.countG++; if (shared.countG < 0 || shared.countG > shared.delayG) shared.countG = 0;
                    shared.countH++; if (shared.countH < 0 || shared.countH > shared.delayH) shared.countH = 0;

                    double outEL = channel[0].aE[shared.countE-((shared.countE > shared.delayE)?shared.delayE+1:0)];
                    double outFL = channel[0].aF[shared.countF-((shared.countF > shared.delayF)?shared.delayF+1:0)];
                    double outGL = channel[0].aG[shared.countG-((shared.countG > shared.delayG)?shared.delayG+1:0)];
                    double outHL = channel[0].aH[shared.countH-((shared.countH > shared.delayH)?shared.delayH+1:0)];
                    double outER = channel[1].aE[shared.countE-((shared.countE > shared.delayE)?shared.delayE+1:0)];
                    double outFR = channel[1].aF[shared.countF-((shared.countF > shared.delayF)?shared.delayF+1:0)];
                    double outGR = channel[1].aG[shared.countG-((shared.countG > shared.delayG)?shared.delayG+1:0)];
                    double outHR = channel[1].aH[shared.countH-((shared.countH > shared.delayH)?shared.delayH+1:0)];

                    channel[0].feedbackA = (outEL - (outFL + outGL + outHL));
                    channel[0].feedbackB = (outFL - (outEL + outGL + outHL));
                    channel[0].feedbackC = (outGL - (outEL + outFL + outHL));
                    channel[0].feedbackD = (outHL - (outEL + outFL + outGL));
                    channel[1].feedbackA = (outER - (outFR + outGR + outHR));
                    channel[1].feedbackB = (outFR - (outER + outGR + outHR));
                    channel[1].feedbackC = (outGR - (outER + outFR + outHR));
                    channel[1].feedbackD = (outHR - (outER + outFR + outGR));

                    inputSampleL = (outEL + outFL + outGL + outHL)/8;
                    inputSampleR = (outER + outFR + outGR + outHR)/8;
                    if (cycleEnd == 4)
                    {
                        channel[0].lastRef[0] = channel[0].lastRef[4]; //start from previous last
                        channel[0].lastRef[2] = (channel[0].lastRef[0] + inputSampleL)/2; //half
                        channel[0].lastRef[1] = (channel[0].lastRef[0] + channel[0].lastRef[2])/2; //one quarter
                        channel[0].lastRef[3] = (channel[0].lastRef[2] + inputSampleL)/2; //three quarters
                        channel[0].lastRef[4] = inputSampleL; //full
                        channel[1].lastRef[0] = channel[1].lastRef[4]; //start from previous last
                        channel[1].lastRef[2] = (channel[1].lastRef[0] + inputSampleR)/2; //half
                        channel[1].lastRef[1] = (channel[1].lastRef[0] + channel[1].lastRef[2])/2; //one quarter
                        channel[1].lastRef[3] = (channel[1].lastRef[2] + inputSampleR)/2; //three quarters
                        channel[1].lastRef[4] = inputSampleR; //full
                    }
                    if (cycleEnd == 3)
                    {
                        channel[0].lastRef[0] = channel[0].lastRef[3]; //start from previous last
                        channel[0].lastRef[2] = (channel[0].lastRef[0]+channel[0].lastRef[0]+inputSampleL)/3; //third
                        channel[0].lastRef[1] = (channel[0].lastRef[0]+inputSampleL+inputSampleL)/3; //two thirds
                        channel[0].lastRef[3] = inputSampleL; //full
                        channel[1].lastRef[0] = channel[1].lastRef[3]; //start from previous last
                        channel[1].lastRef[2] = (channel[1].lastRef[0]+channel[1].lastRef[0]+inputSampleR)/3; //third
                        channel[1].lastRef[1] = (channel[1].lastRef[0]+inputSampleR+inputSampleR)/3; //two thirds
                        channel[1].lastRef[3] = inputSampleR; //full
                    }
                    if (cycleEnd == 2)
                    {
                        channel[0].lastRef[0] = channel[0].lastRef[2]; //start from previous last
                        channel[0].lastRef[1] = (channel[0].lastRef[0] + inputSampleL)/2; //half
                        channel[0].lastRef[2] = inputSampleL; //full
                        channel[1].lastRef[0] = channel[1].lastRef[2]; //start from previous last
                        channel[1].lastRef[1] = (channel[1].lastRef[0] + inputSampleR)/2; //half
                        channel[1].lastRef[2] = inputSampleR; //full
                    }
                    if (cycleEnd == 1)
                    {
                        channel[0].lastRef[0] = inputSampleL;
                        channel[1].lastRef[0] = inputSampleR;
                    }
                    shared.cycle = 0; //reset
                    inputSampleL = channel[0].lastRef[shared.cycle];
                    inputSampleR = channel[1].lastRef[shared.cycle];
                }
                else
                {
                    inputSampleL = channel[0].lastRef[shared.cycle];
                    inputSampleR = channel[1].lastRef[shared.cycle];
                    //we are going through our references now
                }

                inputSampleL = channel[0].iirB = (channel[0].iirB*(1.0-lowpass))+(inputSampleL*lowpass);
                inputSampleR = channel[1].iirB = (channel[1].iirB*(1.0-lowpass))+(inputSampleR*lowpass);
                //end filter

                if (wet < 1.0)
                {
                    inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
                    inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
                }

                //begin 32 bit stereo floating point dither
                int expon; frexpf((float)inputSampleL, &expon);
                channel[0].fpd ^= channel[0].fpd << 13; channel[0].fpd ^= channel[0].fpd >> 17; channel[0].fpd ^= channel[0].fpd << 5;
                inputSampleL += ((double(channel[0].fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
                frexpf((float)inputSampleR, &expon);
                channel[1].fpd ^= channel[1].fpd << 13; channel[1].fpd ^= channel[1].fpd >> 17; channel[1].fpd ^= channel[1].fpd << 5;
                inputSampleR += ((double(channel[1].fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
                //end 32 bit stereo floating point dither

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
