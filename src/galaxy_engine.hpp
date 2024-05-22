// Sapphire Galaxy - Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire project:
// https://github.com/cosinekitty/sapphire
//
// This reverb is an adaptation of Airwindows Galactic by Chris Johnson:
// https://github.com/airwindows/airwindows

#pragma once
#include <cstdint>
#include <stdexcept>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    namespace Galaxy
    {
        const int NDELAYS = 13;
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

            buffer_t& tank(int channel, int tankIndex)
            {
                switch (channel)
                {
                case 0:
                    return L.array[tankIndex];
                case 1:
                    return R.array[tankIndex];
                default:
                    throw std::runtime_error("Invalid channel");
                }
            }

            DelayState& dstate(int tankIndex)
            {
                if (tankIndex < 0 || tankIndex >= NDELAYS)
                    throw std::out_of_range(std::string("tankIndex is invalid: ") + std::to_string(tankIndex));
                return delay[tankIndex];
            }

            double& access(int channel, int tankIndex, int sampleIndex)
            {
                return tank(channel, tankIndex).at(sampleIndex);
            }

            double& head(int channel, int tankIndex)
            {
                return access(channel, tankIndex, dstate(tankIndex).count);
            }

            double& tail(int channel, int tankIndex)
            {
                return access(channel, tankIndex, dstate(tankIndex).tail());
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

                head(0, 12) = inputSampleL * attenuate;
                head(1, 12) = inputSampleR * attenuate;
                delay[12].advance();

                double offsetML = (std::sin(vibM)+1)*127;
                double fracML = offsetML - std::floor(offsetML);
                int workingML = delay[12].count + offsetML;
                double interpolML =
                    access(0, 12, delay[12].reverse(workingML)) * (1-fracML) +
                    access(0, 12, delay[12].reverse(workingML+1)) * (fracML);

                double offsetMR = (std::sin(vibM+M_PI_2)+1)*127;
                double fracMR = offsetMR - std::floor(offsetMR);
                int workingMR = delay[12].count + offsetMR;
                double interpolMR =
                    access(1, 12, delay[12].reverse(workingMR)) * (1-fracMR) +
                    access(1, 12, delay[12].reverse(workingMR+1)) * (fracMR);

                inputSampleL = L.iirA = (L.iirA*(1-lowpass))+(interpolML*lowpass);
                inputSampleR = R.iirA = (R.iirA*(1-lowpass))+(interpolMR*lowpass);

                if (++cycle == cycleEnd)
                {
                    head(0, 8) = inputSampleL + (R.feedbackA * regen);
                    head(1, 8) = inputSampleR + (L.feedbackA * regen);
                    delay[ 8].advance();

                    head(0, 9) = inputSampleL + (R.feedbackB * regen);
                    head(1, 9) = inputSampleR + (L.feedbackB * regen);
                    delay[ 9].advance();

                    head(0, 10) = inputSampleL + (R.feedbackC * regen);
                    head(1, 10) = inputSampleR + (L.feedbackC * regen);
                    delay[10].advance();

                    head(0, 11) = inputSampleL + (R.feedbackD * regen);
                    head(1, 11) = inputSampleR + (L.feedbackD * regen);
                    delay[11].advance();

                    double outIL = tail(0,  8);
                    double outJL = tail(0,  9);
                    double outKL = tail(0, 10);
                    double outLL = tail(0, 11);
                    double outIR = tail(1,  8);
                    double outJR = tail(1,  9);
                    double outKR = tail(1, 10);
                    double outLR = tail(1, 11);

                    head(0, 0) = (outIL - (outJL + outKL + outLL));
                    head(1, 0) = (outIR - (outJR + outKR + outLR));
                    delay[0].advance();

                    head(0, 1) = (outJL - (outIL + outKL + outLL));
                    head(1, 1) = (outJR - (outIR + outKR + outLR));
                    delay[1].advance();

                    head(0, 2) = (outKL - (outIL + outJL + outLL));
                    head(1, 2) = (outKR - (outIR + outJR + outLR));
                    delay[2].advance();

                    head(0, 3) = (outLL - (outIL + outJL + outKL));
                    head(1, 3) = (outLR - (outIR + outJR + outKR));
                    delay[3].advance();

                    double outAL = tail(0, 0);
                    double outBL = tail(0, 1);
                    double outCL = tail(0, 2);
                    double outDL = tail(0, 3);
                    double outAR = tail(1, 0);
                    double outBR = tail(1, 1);
                    double outCR = tail(1, 2);
                    double outDR = tail(1, 3);

                    head(0, 4) = (outAL - (outBL + outCL + outDL));
                    head(1, 4) = (outAR - (outBR + outCR + outDR));
                    delay[4].advance();

                    head(0, 5) = (outBL - (outAL + outCL + outDL));
                    head(1, 5) = (outBR - (outAR + outCR + outDR));
                    delay[5].advance();

                    head(0, 6) = (outCL - (outAL + outBL + outDL));
                    head(1, 6) = (outCR - (outAR + outBR + outDR));
                    delay[6].advance();

                    head(0, 7) = (outDL - (outAL + outBL + outCL));
                    head(1, 7) = (outDR - (outAR + outBR + outCR));
                    delay[7].advance();

                    double outEL = tail(0, 4);
                    double outFL = tail(0, 5);
                    double outGL = tail(0, 6);
                    double outHL = tail(0, 7);
                    double outER = tail(1, 4);
                    double outFR = tail(1, 5);
                    double outGR = tail(1, 6);
                    double outHR = tail(1, 7);

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
