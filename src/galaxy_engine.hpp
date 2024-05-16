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


        struct DelayState
        {
            int count;
            int delay;

            void advance()
            {
                ++count;
                if (count < 0 || count > delay)
                    count = 0;
            }

            int offset(int length) const
            {
                return (length > delay) ? (delay + 1) : 0;
            }

            int reverse(int length) const
            {
                return length - offset(length);
            }

            int tail() const
            {
                return reverse(count);
            }
        };


        struct SharedState
        {
            double depthM;
            double vibM;
            double oldfpd;
            int cycle;
            DelayState A, B, C, D, E, F, G, H, I, J, K, L, M;
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

                S.A.delay = 4801.0*size;
                S.B.delay = 2909.0*size;
                S.C.delay = 1153.0*size;
                S.D.delay = 461.0*size;
                S.E.delay = 7607.0*size;
                S.F.delay = 4217.0*size;
                S.G.delay = 2269.0*size;
                S.H.delay = 1597.0*size;
                S.I.delay = 3407.0*size;
                S.J.delay = 1823.0*size;
                S.K.delay = 859.0*size;
                S.L.delay = 331.0*size;
                S.M.delay = 256;

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

                L.aM[S.M.count] = inputSampleL * attenuate;
                R.aM[S.M.count] = inputSampleR * attenuate;
                S.M.advance();

                double offsetML = (std::sin(S.vibM)+1.0)*127;
                double offsetMR = (std::sin(S.vibM+M_PI_2)+1.0)*127;
                int workingML = S.M.count + offsetML;
                int workingMR = S.M.count + offsetMR;
                double interpolML = (L.aM[S.M.reverse(workingML)] * (1-(offsetML-std::floor(offsetML))));
                interpolML += (L.aM[S.M.reverse(workingML+1)] * ((offsetML-std::floor(offsetML))) );
                double interpolMR = (R.aM[S.M.reverse(workingMR)] * (1-(offsetMR-floor(offsetMR))));
                interpolMR += (R.aM[S.M.reverse(workingMR+1)] * ((offsetMR-floor(offsetMR))) );
                inputSampleL = L.iirA = (L.iirA*(1-lowpass))+(interpolML*lowpass);
                inputSampleR = R.iirA = (R.iirA*(1-lowpass))+(interpolMR*lowpass);

                if (++S.cycle == cycleEnd)
                {
                    L.aI[S.I.count] = inputSampleL + (L.feedbackA * regen);
                    L.aJ[S.J.count] = inputSampleL + (L.feedbackB * regen);
                    L.aK[S.K.count] = inputSampleL + (L.feedbackC * regen);
                    L.aL[S.L.count] = inputSampleL + (L.feedbackD * regen);
                    R.aI[S.I.count] = inputSampleR + (R.feedbackA * regen);
                    R.aJ[S.J.count] = inputSampleR + (R.feedbackB * regen);
                    R.aK[S.K.count] = inputSampleR + (R.feedbackC * regen);
                    R.aL[S.L.count] = inputSampleR + (R.feedbackD * regen);

                    S.I.advance();
                    S.J.advance();
                    S.K.advance();
                    S.L.advance();

                    double outIL = L.aI[S.I.tail()];
                    double outJL = L.aJ[S.J.tail()];
                    double outKL = L.aK[S.K.tail()];
                    double outLL = L.aL[S.L.tail()];
                    double outIR = R.aI[S.I.tail()];
                    double outJR = R.aJ[S.J.tail()];
                    double outKR = R.aK[S.K.tail()];
                    double outLR = R.aL[S.L.tail()];

                    L.aA[S.A.count] = (outIL - (outJL + outKL + outLL));
                    L.aB[S.B.count] = (outJL - (outIL + outKL + outLL));
                    L.aC[S.C.count] = (outKL - (outIL + outJL + outLL));
                    L.aD[S.D.count] = (outLL - (outIL + outJL + outKL));
                    R.aA[S.A.count] = (outIR - (outJR + outKR + outLR));
                    R.aB[S.B.count] = (outJR - (outIR + outKR + outLR));
                    R.aC[S.C.count] = (outKR - (outIR + outJR + outLR));
                    R.aD[S.D.count] = (outLR - (outIR + outJR + outKR));

                    S.A.advance();
                    S.B.advance();
                    S.C.advance();
                    S.D.advance();

                    double outAL = L.aA[S.A.tail()];
                    double outBL = L.aB[S.B.tail()];
                    double outCL = L.aC[S.C.tail()];
                    double outDL = L.aD[S.D.tail()];
                    double outAR = R.aA[S.A.tail()];
                    double outBR = R.aB[S.B.tail()];
                    double outCR = R.aC[S.C.tail()];
                    double outDR = R.aD[S.D.tail()];

                    L.aE[S.E.count] = (outAL - (outBL + outCL + outDL));
                    L.aF[S.F.count] = (outBL - (outAL + outCL + outDL));
                    L.aG[S.G.count] = (outCL - (outAL + outBL + outDL));
                    L.aH[S.H.count] = (outDL - (outAL + outBL + outCL));
                    R.aE[S.E.count] = (outAR - (outBR + outCR + outDR));
                    R.aF[S.F.count] = (outBR - (outAR + outCR + outDR));
                    R.aG[S.G.count] = (outCR - (outAR + outBR + outDR));
                    R.aH[S.H.count] = (outDR - (outAR + outBR + outCR));

                    S.E.advance();
                    S.F.advance();
                    S.G.advance();
                    S.H.advance();

                    double outEL = L.aE[S.E.tail()];
                    double outFL = L.aF[S.F.tail()];
                    double outGL = L.aG[S.G.tail()];
                    double outHL = L.aH[S.H.tail()];
                    double outER = R.aE[S.E.tail()];
                    double outFR = R.aF[S.F.tail()];
                    double outGR = R.aG[S.G.tail()];
                    double outHR = R.aH[S.H.tail()];

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
