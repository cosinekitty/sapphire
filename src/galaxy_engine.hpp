// Sapphire Galaxy - Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire project:
// https://github.com/cosinekitty/sapphire
//
// This reverb is an adaptation of Airwindows Galactic by Chris Johnson:
// https://github.com/airwindows/airwindows

#pragma once
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

        const float ParamKnobMin = 0.0;
        const float ParamKnobDef = 0.5;
        const float ParamKnobMax = 1.0;

        inline float ParamClamp(float x)
        {
            if (!std::isfinite(x))
                return ParamKnobDef;

            return std::clamp(x, ParamKnobMin, ParamKnobMax);
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


        struct Engine
        {
        private:
            // Parameters
            float replaceKnob = 0.5;
            float brightKnob = 0.5;
            float detuneKnob = 0.5;
            float bignessKnob = 1.0;
            float mixKnob = 1.0;

            // State
            ChannelState L{2756923396};
            ChannelState R{2341963165};
            double depthM;
            double vibM;
            double oldfpd;
            int cycle;
            DelayState delay[NDELAYS];

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

            Engine()
            {
                initialize();
            }

            void initialize()
            {
                L.clear();
                R.clear();
                depthM = 0;
                vibM = 3;
                oldfpd = 429496.7295;
                cycle = 0;
                for (int i = 0; i < NDELAYS; ++i)
                    delay[i].clear();
            }

            double getReplace()     const { return replaceKnob; }
            double getBrightness()  const { return brightKnob; }
            double getDetune()      const { return detuneKnob; }
            double getBigness()     const { return bignessKnob; }
            double getMix()         const { return mixKnob; }

            void setReplace(double replace)         { replaceKnob = ParamClamp(replace);    }
            void setBrightness(double brightness)   { brightKnob  = ParamClamp(brightness); }
            void setDetune(double detune)           { detuneKnob  = ParamClamp(detune);     }
            void setBigness(double bigness)         { bignessKnob = ParamClamp(bigness);    }
            void setMix(double mix)                 { mixKnob     = ParamClamp(mix);        }

            void process(float sampleRate, float inLeft, float inRight, float& outLeft, float& outRight)
            {
                double resultLeft, resultRight;
                process(sampleRate, inLeft, inRight, resultLeft, resultRight);
                outLeft = static_cast<float>(resultLeft);
                outRight = static_cast<float>(resultRight);
            }

            void process(double sampleRateHz, double inputSampleL, double inputSampleR, double& outputSampleL, double& outputSampleR)
            {
                const double overallscale = sampleRateHz / 44100;
                const int cycleEnd = std::clamp(static_cast<int>(std::floor(overallscale)), MinCycle, MaxCycle);

                if (cycle > cycleEnd-1)
                    cycle = cycleEnd-1;

                const double regen = 0.0625+((1.0-replaceKnob)*0.0625);
                const double attenuate = (1.0 - (regen / 0.125))*1.333;
                const double lowpass = Square(1.00001-(1.0-brightKnob))/std::sqrt(overallscale);
                const double drift = Cube(detuneKnob)*0.001;
                const double size = (bignessKnob*1.77)+0.1;
                const double wet = 1-Cube(1 - mixKnob);

                delay[ 0].delay = 4801*size;
                delay[ 1].delay = 2909*size;
                delay[ 2].delay = 1153*size;
                delay[ 3].delay =  461*size;
                delay[ 4].delay = 7607*size;
                delay[ 5].delay = 4217*size;
                delay[ 6].delay = 2269*size;
                delay[ 7].delay = 1597*size;
                delay[ 8].delay = 3407*size;
                delay[ 9].delay = 1823*size;
                delay[10].delay =  859*size;
                delay[11].delay =  331*size;
                delay[12].delay =  256;

                if (std::abs(inputSampleL)<1.18e-23) inputSampleL = L.fpd * 1.18e-17;
                if (std::abs(inputSampleR)<1.18e-23) inputSampleR = R.fpd * 1.18e-17;
                const double drySampleL = inputSampleL;
                const double drySampleR = inputSampleR;

                vibM += (oldfpd*drift);
                if (vibM > 2*M_PI)
                {
                    vibM = 0;
                    oldfpd = 0.4294967295+(L.fpd*0.0000000000618);
                }

                L.array[12].at(delay[12].count) = inputSampleL * attenuate;
                R.array[12].at(delay[12].count) = inputSampleR * attenuate;
                delay[12].advance();

                double offsetML = (std::sin(vibM)+1)*127;
                double offsetMR = (std::sin(vibM+M_PI_2)+1)*127;
                int workingML = delay[12].count + offsetML;
                int workingMR = delay[12].count + offsetMR;
                double interpolML = (L.array[12].at(delay[12].reverse(workingML)) * (1-(offsetML-std::floor(offsetML))));
                interpolML += (L.array[12].at(delay[12].reverse(workingML+1)) * ((offsetML-std::floor(offsetML))) );
                double interpolMR = (R.array[12].at(delay[12].reverse(workingMR)) * (1-(offsetMR-std::floor(offsetMR))));
                interpolMR += (R.array[12].at(delay[12].reverse(workingMR+1)) * ((offsetMR-std::floor(offsetMR))));
                inputSampleL = L.iirA = (L.iirA*(1-lowpass))+(interpolML*lowpass);
                inputSampleR = R.iirA = (R.iirA*(1-lowpass))+(interpolMR*lowpass);

                if (++cycle == cycleEnd)
                {
                    L.array[ 8].at(delay[ 8].count) = inputSampleL + (R.feedbackA * regen);
                    L.array[ 9].at(delay[ 9].count) = inputSampleL + (R.feedbackB * regen);
                    L.array[10].at(delay[10].count) = inputSampleL + (R.feedbackC * regen);
                    L.array[11].at(delay[11].count) = inputSampleL + (R.feedbackD * regen);
                    R.array[ 8].at(delay[ 8].count) = inputSampleR + (L.feedbackA * regen);
                    R.array[ 9].at(delay[ 9].count) = inputSampleR + (L.feedbackB * regen);
                    R.array[10].at(delay[10].count) = inputSampleR + (L.feedbackC * regen);
                    R.array[11].at(delay[11].count) = inputSampleR + (L.feedbackD * regen);

                    delay[ 8].advance();
                    delay[ 9].advance();
                    delay[10].advance();
                    delay[11].advance();

                    double outIL = L.array[ 8].at(delay[ 8].tail());
                    double outJL = L.array[ 9].at(delay[ 9].tail());
                    double outKL = L.array[10].at(delay[10].tail());
                    double outLL = L.array[11].at(delay[11].tail());
                    double outIR = R.array[ 8].at(delay[ 8].tail());
                    double outJR = R.array[ 9].at(delay[ 9].tail());
                    double outKR = R.array[10].at(delay[10].tail());
                    double outLR = R.array[11].at(delay[11].tail());

                    L.array[0].at(delay[0].count) = (outIL - (outJL + outKL + outLL));
                    L.array[1].at(delay[1].count) = (outJL - (outIL + outKL + outLL));
                    L.array[2].at(delay[2].count) = (outKL - (outIL + outJL + outLL));
                    L.array[3].at(delay[3].count) = (outLL - (outIL + outJL + outKL));
                    R.array[0].at(delay[0].count) = (outIR - (outJR + outKR + outLR));
                    R.array[1].at(delay[1].count) = (outJR - (outIR + outKR + outLR));
                    R.array[2].at(delay[2].count) = (outKR - (outIR + outJR + outLR));
                    R.array[3].at(delay[3].count) = (outLR - (outIR + outJR + outKR));

                    delay[0].advance();
                    delay[1].advance();
                    delay[2].advance();
                    delay[3].advance();

                    double outAL = L.array[0].at(delay[0].tail());
                    double outBL = L.array[1].at(delay[1].tail());
                    double outCL = L.array[2].at(delay[2].tail());
                    double outDL = L.array[3].at(delay[3].tail());
                    double outAR = R.array[0].at(delay[0].tail());
                    double outBR = R.array[1].at(delay[1].tail());
                    double outCR = R.array[2].at(delay[2].tail());
                    double outDR = R.array[3].at(delay[3].tail());

                    L.array[4].at(delay[4].count) = (outAL - (outBL + outCL + outDL));
                    L.array[5].at(delay[5].count) = (outBL - (outAL + outCL + outDL));
                    L.array[6].at(delay[6].count) = (outCL - (outAL + outBL + outDL));
                    L.array[7].at(delay[7].count) = (outDL - (outAL + outBL + outCL));
                    R.array[4].at(delay[4].count) = (outAR - (outBR + outCR + outDR));
                    R.array[5].at(delay[5].count) = (outBR - (outAR + outCR + outDR));
                    R.array[6].at(delay[6].count) = (outCR - (outAR + outBR + outDR));
                    R.array[7].at(delay[7].count) = (outDR - (outAR + outBR + outCR));

                    delay[4].advance();
                    delay[5].advance();
                    delay[6].advance();
                    delay[7].advance();

                    double outEL = L.array[4].at(delay[4].tail());
                    double outFL = L.array[5].at(delay[5].tail());
                    double outGL = L.array[6].at(delay[6].tail());
                    double outHL = L.array[7].at(delay[7].tail());
                    double outER = R.array[4].at(delay[4].tail());
                    double outFR = R.array[5].at(delay[5].tail());
                    double outGR = R.array[6].at(delay[6].tail());
                    double outHR = R.array[7].at(delay[7].tail());

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
                    cycle = 0;
                }
                inputSampleL = L.iirB = (L.iirB*(1-lowpass))+(L.lastRef[cycle]*lowpass);
                inputSampleR = R.iirB = (R.iirB*(1-lowpass))+(R.lastRef[cycle]*lowpass);
                if (wet < 1.0)
                {
                    inputSampleL = (inputSampleL * wet) + (drySampleL * (1-wet));
                    inputSampleR = (inputSampleR * wet) + (drySampleR * (1-wet));
                }
                outputSampleL = dither(inputSampleL, L.fpd);
                outputSampleR = dither(inputSampleR, R.fpd);
            }
        };
    }
}
