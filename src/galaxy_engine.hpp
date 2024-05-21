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
        const int NDELAYS = 13;     // A=0, B=1, C=2, D=3, E=4, F=5, G=6, H=7, I=8, J=9, K=10, L=11, M=12.

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

        using buffer_t = std::vector<double>;

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
            buffer_t array[NDELAYS];

            explicit ChannelState(uint32_t _init_fpd)
                : init_fpd(_init_fpd)
            {
                array[ 0].resize( 9700);
                array[ 1].resize( 6000);
                array[ 2].resize( 2320);
                array[ 3].resize(  940);
                array[ 4].resize(15220);
                array[ 5].resize( 8460);
                array[ 6].resize( 4540);
                array[ 7].resize( 3200);
                array[ 8].resize( 6480);
                array[ 9].resize( 3660);
                array[10].resize( 1720);
                array[11].resize(  680);
                array[12].resize( 3111);

                clear();
            }

            void clear()
            {
                iirA = iirB = 0;
                feedbackA = feedbackB = feedbackC = feedbackD = 0;
                fpd = init_fpd;
                for (int i = 0; i <= MaxCycle; ++i)
                    lastRef[i] = 0;
                thunder = 0;

                for (int i = 0; i < NDELAYS; ++i)
                    for (double& x : array[i])
                        x = 0;
            }
        };


        struct DelayState
        {
            int count{};
            int delay{};    // written on every process() call

            void clear()
            {
                count = 1;
            }

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
            DelayState delay[NDELAYS];

            void clear()
            {
                depthM = 0;
                vibM = 3;
                oldfpd = 429496.7295;
                cycle = 0;
                for (int i = 0; i < NDELAYS; ++i)
                    delay[i].clear();
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

                S.delay[ 0].delay = 4801*size;
                S.delay[ 1].delay = 2909*size;
                S.delay[ 2].delay = 1153*size;
                S.delay[ 3].delay =  461*size;
                S.delay[ 4].delay = 7607*size;
                S.delay[ 5].delay = 4217*size;
                S.delay[ 6].delay = 2269*size;
                S.delay[ 7].delay = 1597*size;
                S.delay[ 8].delay = 3407*size;
                S.delay[ 9].delay = 1823*size;
                S.delay[10].delay =  859*size;
                S.delay[11].delay =  331*size;
                S.delay[12].delay =  256;

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

                L.array[12].at(S.delay[12].count) = inputSampleL * attenuate;
                R.array[12].at(S.delay[12].count) = inputSampleR * attenuate;
                S.delay[12].advance();

                double offsetML = (std::sin(S.vibM)+1)*127;
                double offsetMR = (std::sin(S.vibM+M_PI_2)+1)*127;
                int workingML = S.delay[12].count + offsetML;
                int workingMR = S.delay[12].count + offsetMR;
                double interpolML = (L.array[12].at(S.delay[12].reverse(workingML)) * (1-(offsetML-std::floor(offsetML))));
                interpolML += (L.array[12].at(S.delay[12].reverse(workingML+1)) * ((offsetML-std::floor(offsetML))) );
                double interpolMR = (R.array[12].at(S.delay[12].reverse(workingMR)) * (1-(offsetMR-std::floor(offsetMR))));
                interpolMR += (R.array[12].at(S.delay[12].reverse(workingMR+1)) * ((offsetMR-std::floor(offsetMR))));
                inputSampleL = L.iirA = (L.iirA*(1-lowpass))+(interpolML*lowpass);
                inputSampleR = R.iirA = (R.iirA*(1-lowpass))+(interpolMR*lowpass);

                if (++S.cycle == cycleEnd)
                {
                    L.array[ 8].at(S.delay[ 8].count) = inputSampleL + (R.feedbackA * regen);
                    L.array[ 9].at(S.delay[ 9].count) = inputSampleL + (R.feedbackB * regen);
                    L.array[10].at(S.delay[10].count) = inputSampleL + (R.feedbackC * regen);
                    L.array[11].at(S.delay[11].count) = inputSampleL + (R.feedbackD * regen);
                    R.array[ 8].at(S.delay[ 8].count) = inputSampleR + (L.feedbackA * regen);
                    R.array[ 9].at(S.delay[ 9].count) = inputSampleR + (L.feedbackB * regen);
                    R.array[10].at(S.delay[10].count) = inputSampleR + (L.feedbackC * regen);
                    R.array[11].at(S.delay[11].count) = inputSampleR + (L.feedbackD * regen);

                    S.delay[ 8].advance();
                    S.delay[ 9].advance();
                    S.delay[10].advance();
                    S.delay[11].advance();

                    double outIL = L.array[ 8].at(S.delay[ 8].tail());
                    double outJL = L.array[ 9].at(S.delay[ 9].tail());
                    double outKL = L.array[10].at(S.delay[10].tail());
                    double outLL = L.array[11].at(S.delay[11].tail());
                    double outIR = R.array[ 8].at(S.delay[ 8].tail());
                    double outJR = R.array[ 9].at(S.delay[ 9].tail());
                    double outKR = R.array[10].at(S.delay[10].tail());
                    double outLR = R.array[11].at(S.delay[11].tail());

                    L.array[0].at(S.delay[0].count) = (outIL - (outJL + outKL + outLL));
                    L.array[1].at(S.delay[1].count) = (outJL - (outIL + outKL + outLL));
                    L.array[2].at(S.delay[2].count) = (outKL - (outIL + outJL + outLL));
                    L.array[3].at(S.delay[3].count) = (outLL - (outIL + outJL + outKL));
                    R.array[0].at(S.delay[0].count) = (outIR - (outJR + outKR + outLR));
                    R.array[1].at(S.delay[1].count) = (outJR - (outIR + outKR + outLR));
                    R.array[2].at(S.delay[2].count) = (outKR - (outIR + outJR + outLR));
                    R.array[3].at(S.delay[3].count) = (outLR - (outIR + outJR + outKR));

                    S.delay[0].advance();
                    S.delay[1].advance();
                    S.delay[2].advance();
                    S.delay[3].advance();

                    double outAL = L.array[0].at(S.delay[0].tail());
                    double outBL = L.array[1].at(S.delay[1].tail());
                    double outCL = L.array[2].at(S.delay[2].tail());
                    double outDL = L.array[3].at(S.delay[3].tail());
                    double outAR = R.array[0].at(S.delay[0].tail());
                    double outBR = R.array[1].at(S.delay[1].tail());
                    double outCR = R.array[2].at(S.delay[2].tail());
                    double outDR = R.array[3].at(S.delay[3].tail());

                    L.array[4].at(S.delay[4].count) = (outAL - (outBL + outCL + outDL));
                    L.array[5].at(S.delay[5].count) = (outBL - (outAL + outCL + outDL));
                    L.array[6].at(S.delay[6].count) = (outCL - (outAL + outBL + outDL));
                    L.array[7].at(S.delay[7].count) = (outDL - (outAL + outBL + outCL));
                    R.array[4].at(S.delay[4].count) = (outAR - (outBR + outCR + outDR));
                    R.array[5].at(S.delay[5].count) = (outBR - (outAR + outCR + outDR));
                    R.array[6].at(S.delay[6].count) = (outCR - (outAR + outBR + outDR));
                    R.array[7].at(S.delay[7].count) = (outDR - (outAR + outBR + outCR));

                    S.delay[4].advance();
                    S.delay[5].advance();
                    S.delay[6].advance();
                    S.delay[7].advance();

                    double outEL = L.array[4].at(S.delay[4].tail());
                    double outFL = L.array[5].at(S.delay[5].tail());
                    double outGL = L.array[6].at(S.delay[6].tail());
                    double outHL = L.array[7].at(S.delay[7].tail());
                    double outER = R.array[4].at(S.delay[4].tail());
                    double outFR = R.array[5].at(S.delay[5].tail());
                    double outGR = R.array[6].at(S.delay[6].tail());
                    double outHR = R.array[7].at(S.delay[7].tail());

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
