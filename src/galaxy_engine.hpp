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
        const int MinCycle = 1;
        const int MaxCycle = 4;

        inline double Square(double x)
        {
            return x * x;
        }

        inline double Cube(double x)
        {
            return x * x * x;
        }

        struct ChannelState
        {
            double iirA;
            double iirB;
            double feedbackA;
            double feedbackB;
            double feedbackC;
            double feedbackD;
            const uint32_t init_fpd;
            uint32_t fpd;
            double lastRef[MaxCycle+1];
            double thunder;
            std::vector<double> aA;
            std::vector<double> aB;
            std::vector<double> aC;
            std::vector<double> aD;
            std::vector<double> aE;
            std::vector<double> aF;
            std::vector<double> aG;
            std::vector<double> aH;
            std::vector<double> aI;
            std::vector<double> aJ;
            std::vector<double> aK;
            std::vector<double> aL;
            std::vector<double> aM;

            explicit ChannelState(uint32_t _init_fpd)
                : init_fpd(_init_fpd)
            {
                aA.resize( 9700);
                aB.resize( 6000);
                aC.resize( 2320);
                aD.resize(  940);
                aE.resize(15220);
                aF.resize( 8460);
                aG.resize( 4540);
                aH.resize( 3200);
                aI.resize( 6480);
                aJ.resize( 3660);
                aK.resize( 1720);
                aL.resize(  680);
                aM.resize( 3111);

                clearInternal();
            }

            void clearInternal()
            {
                iirA = iirB = 0;
                feedbackA = feedbackB = feedbackC = feedbackD = 0;
                fpd = init_fpd;
                for (int i = 0; i <= MaxCycle; ++i)
                    lastRef[i] = 0;
                thunder = 0;
            }

            void clear()
            {
                clearInternal();
                clearBuffer(aA);
                clearBuffer(aB);
                clearBuffer(aC);
                clearBuffer(aD);
                clearBuffer(aE);
                clearBuffer(aF);
                clearBuffer(aG);
                clearBuffer(aH);
                clearBuffer(aI);
                clearBuffer(aJ);
                clearBuffer(aK);
                clearBuffer(aL);
                clearBuffer(aM);
            }

            static void clearBuffer(std::vector<double>& vec)
            {
                for (double& x : vec)
                    x = 0;
            }
        };


        struct DelayState
        {
            int count{};
            int delay{};

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

            void clear()
            {
                depthM = 0;
                vibM = 3;
                oldfpd = 429496.7295;
                cycle = 0;
            }
        };


        struct ReverbParameters
        {
            float A = 0.5;
            float B = 0.5;
            float C = 0.5;
            float D = 1.0;
            float E = 1.0;
        };


        struct ReverbState
        {
        private:
            ChannelState L{2756923396};
            ChannelState R{2341963165};
            SharedState  S;

            static inline double dither(double sample, uint32_t& fpd)
            {
                int expon;
                frexpf((float)sample, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                return sample + ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * std::pow((double)2, (double)expon+62));
            }

        public:
            ReverbParameters parm;

            ReverbState()
            {
                clear();
            }

            void clear()
            {
                L.clear();
                R.clear();
                S.clear();
            }

            void process(double sampleRateHz, double inputSampleL, double inputSampleR, double& outputSampleL, double& outputSampleR)
            {
                const double overallscale = sampleRateHz / 44100;
                const int cycleEnd = std::clamp(static_cast<int>(std::floor(overallscale)), MinCycle, MaxCycle);

                if (S.cycle > cycleEnd-1)
                    S.cycle = cycleEnd-1;

                const double regen = 0.0625+((1.0-parm.A)*0.0625);
                const double attenuate = (1.0 - (regen / 0.125))*1.333;
                const double lowpass = Square(1.00001-(1.0-parm.B))/std::sqrt(overallscale);
                const double drift = Cube(parm.C)*0.001;
                const double size = (parm.D*1.77)+0.1;
                const double wet = 1-Cube(1 - parm.E);

                S.A.delay = 4801*size;
                S.B.delay = 2909*size;
                S.C.delay = 1153*size;
                S.D.delay =  461*size;
                S.E.delay = 7607*size;
                S.F.delay = 4217*size;
                S.G.delay = 2269*size;
                S.H.delay = 1597*size;
                S.I.delay = 3407*size;
                S.J.delay = 1823*size;
                S.K.delay =  859*size;
                S.L.delay =  331*size;
                S.M.delay =  256;

                if (std::abs(inputSampleL)<1.18e-23) inputSampleL = L.fpd * 1.18e-17;
                if (std::abs(inputSampleR)<1.18e-23) inputSampleR = R.fpd * 1.18e-17;
                const double drySampleL = inputSampleL;
                const double drySampleR = inputSampleR;

                S.vibM += (S.oldfpd*drift);
                if (S.vibM > 2*M_PI)
                {
                    S.vibM = 0;
                    S.oldfpd = 0.4294967295+(L.fpd*0.0000000000618);
                }

                L.aM.at(S.M.count) = inputSampleL * attenuate;
                R.aM.at(S.M.count) = inputSampleR * attenuate;
                S.M.advance();

                double offsetML = (std::sin(S.vibM)+1)*127;
                double offsetMR = (std::sin(S.vibM+M_PI_2)+1)*127;
                int workingML = S.M.count + offsetML;
                int workingMR = S.M.count + offsetMR;
                double interpolML = (L.aM.at(S.M.reverse(workingML)) * (1-(offsetML-std::floor(offsetML))));
                interpolML += (L.aM.at(S.M.reverse(workingML+1)) * ((offsetML-std::floor(offsetML))) );
                double interpolMR = (R.aM.at(S.M.reverse(workingMR)) * (1-(offsetMR-std::floor(offsetMR))));
                interpolMR += (R.aM.at(S.M.reverse(workingMR+1)) * ((offsetMR-std::floor(offsetMR))));
                inputSampleL = L.iirA = (L.iirA*(1-lowpass))+(interpolML*lowpass);
                inputSampleR = R.iirA = (R.iirA*(1-lowpass))+(interpolMR*lowpass);

                if (++S.cycle == cycleEnd)
                {
                    L.aI.at(S.I.count) = inputSampleL + (L.feedbackA * regen);
                    L.aJ.at(S.J.count) = inputSampleL + (L.feedbackB * regen);
                    L.aK.at(S.K.count) = inputSampleL + (L.feedbackC * regen);
                    L.aL.at(S.L.count) = inputSampleL + (L.feedbackD * regen);
                    R.aI.at(S.I.count) = inputSampleR + (R.feedbackA * regen);
                    R.aJ.at(S.J.count) = inputSampleR + (R.feedbackB * regen);
                    R.aK.at(S.K.count) = inputSampleR + (R.feedbackC * regen);
                    R.aL.at(S.L.count) = inputSampleR + (R.feedbackD * regen);

                    S.I.advance();
                    S.J.advance();
                    S.K.advance();
                    S.L.advance();

                    double outIL = L.aI.at(S.I.tail());
                    double outJL = L.aJ.at(S.J.tail());
                    double outKL = L.aK.at(S.K.tail());
                    double outLL = L.aL.at(S.L.tail());
                    double outIR = R.aI.at(S.I.tail());
                    double outJR = R.aJ.at(S.J.tail());
                    double outKR = R.aK.at(S.K.tail());
                    double outLR = R.aL.at(S.L.tail());

                    L.aA.at(S.A.count) = (outIL - (outJL + outKL + outLL));
                    L.aB.at(S.B.count) = (outJL - (outIL + outKL + outLL));
                    L.aC.at(S.C.count) = (outKL - (outIL + outJL + outLL));
                    L.aD.at(S.D.count) = (outLL - (outIL + outJL + outKL));
                    R.aA.at(S.A.count) = (outIR - (outJR + outKR + outLR));
                    R.aB.at(S.B.count) = (outJR - (outIR + outKR + outLR));
                    R.aC.at(S.C.count) = (outKR - (outIR + outJR + outLR));
                    R.aD.at(S.D.count) = (outLR - (outIR + outJR + outKR));

                    S.A.advance();
                    S.B.advance();
                    S.C.advance();
                    S.D.advance();

                    double outAL = L.aA.at(S.A.tail());
                    double outBL = L.aB.at(S.B.tail());
                    double outCL = L.aC.at(S.C.tail());
                    double outDL = L.aD.at(S.D.tail());
                    double outAR = R.aA.at(S.A.tail());
                    double outBR = R.aB.at(S.B.tail());
                    double outCR = R.aC.at(S.C.tail());
                    double outDR = R.aD.at(S.D.tail());

                    L.aE.at(S.E.count) = (outAL - (outBL + outCL + outDL));
                    L.aF.at(S.F.count) = (outBL - (outAL + outCL + outDL));
                    L.aG.at(S.G.count) = (outCL - (outAL + outBL + outDL));
                    L.aH.at(S.H.count) = (outDL - (outAL + outBL + outCL));
                    R.aE.at(S.E.count) = (outAR - (outBR + outCR + outDR));
                    R.aF.at(S.F.count) = (outBR - (outAR + outCR + outDR));
                    R.aG.at(S.G.count) = (outCR - (outAR + outBR + outDR));
                    R.aH.at(S.H.count) = (outDR - (outAR + outBR + outCR));

                    S.E.advance();
                    S.F.advance();
                    S.G.advance();
                    S.H.advance();

                    double outEL = L.aE.at(S.E.tail());
                    double outFL = L.aF.at(S.F.tail());
                    double outGL = L.aG.at(S.G.tail());
                    double outHL = L.aH.at(S.H.tail());
                    double outER = R.aE.at(S.E.tail());
                    double outFR = R.aF.at(S.F.tail());
                    double outGR = R.aG.at(S.G.tail());
                    double outHR = R.aH.at(S.H.tail());

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
                inputSampleL = L.iirB = (L.iirB*(1-lowpass))+(L.lastRef[S.cycle]*lowpass);
                inputSampleR = R.iirB = (R.iirB*(1-lowpass))+(R.lastRef[S.cycle]*lowpass);
                if (wet < 1.0)
                {
                    inputSampleL = (inputSampleL * wet) + (drySampleL * (1-wet));
                    inputSampleR = (inputSampleR * wet) + (drySampleR * (1-wet));
                }
                outputSampleL = dither(inputSampleL, L.fpd);
                outputSampleR = dither(inputSampleR, R.fpd);
            }
        };


        const float ParamKnobMin = 0.0;
        const float ParamKnobDef = 0.5;
        const float ParamKnobMax = 1.0;

        inline float ParamClamp(float x)
        {
            if (!std::isfinite(x))
                return ParamKnobDef;

            return std::clamp(x, ParamKnobMin, ParamKnobMax);
        }


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

            double getReplace()     const { return state->parm.A; }
            double getBrightness()  const { return state->parm.B; }
            double getDetune()      const { return state->parm.C; }
            double getBigness()     const { return state->parm.D; }
            double getMix()         const { return state->parm.E; }

            void setReplace(double replace)         { state->parm.A = ParamClamp(replace);    }
            void setBrightness(double brightness)   { state->parm.B = ParamClamp(brightness); }
            void setDetune(double detune)           { state->parm.C = ParamClamp(detune);     }
            void setBigness(double bigness)         { state->parm.D = ParamClamp(bigness);    }
            void setMix(double mix)                 { state->parm.E = ParamClamp(mix);        }

        private:
            std::unique_ptr<ReverbState> state = std::make_unique<ReverbState>();
        };
    }
}
