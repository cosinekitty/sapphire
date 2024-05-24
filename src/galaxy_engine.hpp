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
        const int NFEEDBACK = 4;
        const int MinCycle = 1;
        const int MAXCYCLE = 4;

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

        struct StereoFrame
        {
            double channel[2];

            StereoFrame()
            {
                clear();
            }

            explicit StereoFrame(double left, double right)
            {
                channel[0] = left;
                channel[1] = right;
            }

            void clear()
            {
                channel[0] = channel[1] = 0;
            }

            StereoFrame operator + (const StereoFrame& other) const
            {
                return StereoFrame(
                    channel[0] + other.channel[0],
                    channel[1] + other.channel[1]
                );
            }

            StereoFrame operator - (const StereoFrame& other) const
            {
                return StereoFrame(
                    channel[0] - other.channel[0],
                    channel[1] - other.channel[1]
                );
            }

            StereoFrame operator / (double denom) const
            {
                return StereoFrame(
                    channel[0] / denom,
                    channel[1] / denom
                );
            }

            StereoFrame operator * (double scalar) const
            {
                return StereoFrame(
                    channel[0] * scalar,
                    channel[1] * scalar
                );
            }

            StereoFrame flip() const
            {
                return StereoFrame(channel[1], channel[0]);
            }
        };


        using stereo_buffer_t = std::vector<StereoFrame>;


        struct DelayState
        {
            stereo_buffer_t buffer;
            int count{};
            int delay{};    // written on every process() call

            void clear()
            {
                count = 1;
                for (StereoFrame& frame : buffer)
                    frame.clear();
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
            uint32_t fpd[2];
            double depthM;
            double vibM;
            double oldfpd;
            int cycle;
            DelayState delay[NDELAYS];
            StereoFrame feedback[NFEEDBACK];
            StereoFrame iirA;
            StereoFrame iirB;
            StereoFrame lastRef[MAXCYCLE + 1];

            static inline double dither(double sample, uint32_t& fpd)
            {
                int expon;
                frexpf((float)sample, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                return sample + ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * std::pow((double)2, (double)expon+62));
            }

            DelayState& dstate(int tankIndex)
            {
                if (tankIndex < 0 || tankIndex >= NDELAYS)
                    throw std::out_of_range(std::string("tankIndex is invalid: ") + std::to_string(tankIndex));
                return delay[tankIndex];
            }

            stereo_buffer_t& tank(int tankIndex)
            {
                return dstate(tankIndex).buffer;
            }

            double& access(int channel, int tankIndex, int sampleIndex)
            {
                return tank(tankIndex).at(sampleIndex).channel[channel&1];
            }

            StereoFrame& headFrame(int tankIndex)
            {
                const int headIndex = dstate(tankIndex).count;
                return tank(tankIndex).at(headIndex);
            }

            StereoFrame tailFrame(int tankIndex)
            {
                const int tailIndex = dstate(tankIndex).tail();
                return tank(tankIndex).at(tailIndex);
            }

            void writeFrame(int tankIndex, const StereoFrame& frame)
            {
                headFrame(tankIndex) = frame;
                dstate(tankIndex).advance();
            }

            double interp(int channel, double radians)
            {
                double ofs = (std::sin(vibM + radians) + 1) * 127;
                double frc = ofs - std::floor(ofs);
                int index = delay[12].count + ofs;
                return
                    access(channel, 12, delay[12].reverse(index)) * (1-frc) +
                    access(channel, 12, delay[12].reverse(index+1)) * (frc);
            }

            void loadFrames(StereoFrame f[4], int startIndex)
            {
                f[0] = tailFrame(startIndex+0);
                f[1] = tailFrame(startIndex+1);
                f[2] = tailFrame(startIndex+2);
                f[3] = tailFrame(startIndex+3);
            }

        public:
            Engine()
            {
                delay[ 0].buffer.resize( 9700);
                delay[ 1].buffer.resize( 6000);
                delay[ 2].buffer.resize( 2320);
                delay[ 3].buffer.resize(  940);
                delay[ 4].buffer.resize(15220);
                delay[ 5].buffer.resize( 8460);
                delay[ 6].buffer.resize( 4540);
                delay[ 7].buffer.resize( 3200);
                delay[ 8].buffer.resize( 6480);
                delay[ 9].buffer.resize( 3660);
                delay[10].buffer.resize( 1720);
                delay[11].buffer.resize(  680);
                delay[12].buffer.resize( 3111);
                initialize();
            }

            void initialize()
            {
                fpd[0] = 2756923396;
                fpd[1] = 2341963165;
                depthM = 0;
                vibM = 3;
                oldfpd = 429496.7295;
                cycle = 0;
                for (int i = 0; i < NDELAYS; ++i)
                    delay[i].clear();
                for (int i = 0; i < NFEEDBACK; ++i)
                    feedback[i].clear();
                for (int i = 0; i <= MAXCYCLE; ++i)
                    lastRef[i].clear();
                iirA.clear();
                iirB.clear();
            }

            double getReplace()     const { return replaceKnob; }
            double getBrightness()  const { return brightKnob;  }
            double getDetune()      const { return detuneKnob;  }
            double getBigness()     const { return bignessKnob; }
            double getMix()         const { return mixKnob;     }

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
                const int cycleEnd = std::clamp(static_cast<int>(std::floor(overallscale)), MinCycle, MAXCYCLE);

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

                if (std::abs(inputSampleL) < 1.18e-23) inputSampleL = fpd[0] * 1.18e-17;
                if (std::abs(inputSampleR) < 1.18e-23) inputSampleR = fpd[1] * 1.18e-17;
                const StereoFrame drySample(inputSampleL, inputSampleR);

                vibM += oldfpd * drift;
                if (vibM > 2*M_PI)
                {
                    vibM = 0;
                    oldfpd = 0.4294967295 + (fpd[0] * 0.0000000000618);
                }

                writeFrame(12, drySample * attenuate);

                StereoFrame phasor(interp(0, 0), interp(1, M_PI_2));
                StereoFrame sample = iirA = iirA*(1-lowpass) + phasor*lowpass;

                if (++cycle == cycleEnd)
                {
                    StereoFrame f[4];
                    writeFrame( 8, sample + (feedback[0].flip() * regen));
                    writeFrame( 9, sample + (feedback[1].flip() * regen));
                    writeFrame(10, sample + (feedback[2].flip() * regen));
                    writeFrame(11, sample + (feedback[3].flip() * regen));

                    loadFrames(f, 8);
                    writeFrame(0, f[0] - (f[1] + f[2] + f[3]));
                    writeFrame(1, f[1] - (f[0] + f[2] + f[3]));
                    writeFrame(2, f[2] - (f[0] + f[1] + f[3]));
                    writeFrame(3, f[3] - (f[0] + f[1] + f[2]));

                    loadFrames(f, 0);
                    writeFrame(4, f[0] - (f[1] + f[2] + f[3]));
                    writeFrame(5, f[1] - (f[0] + f[2] + f[3]));
                    writeFrame(6, f[2] - (f[0] + f[1] + f[3]));
                    writeFrame(7, f[3] - (f[0] + f[1] + f[2]));

                    loadFrames(f, 4);
                    feedback[0] = f[0] - (f[1] + f[2] + f[3]);
                    feedback[1] = f[1] - (f[0] + f[2] + f[3]);
                    feedback[2] = f[2] - (f[0] + f[1] + f[3]);
                    feedback[3] = f[3] - (f[0] + f[1] + f[2]);

                    sample = (f[0] + f[1] + f[2] + f[3]) / 8;

                    switch (cycleEnd)
                    {
                    case 4:
                        lastRef[0] = lastRef[4];
                        lastRef[2] = (lastRef[0] + sample)/2;
                        lastRef[1] = (lastRef[0] + lastRef[2])/2;
                        lastRef[3] = (lastRef[2] + sample)/2;
                        lastRef[4] = sample;
                        break;

                    case 3:
                        lastRef[0] = lastRef[3];
                        lastRef[2] = (lastRef[0] + lastRef[0] + sample) / 3;
                        lastRef[1] = (lastRef[0] + sample + sample) / 3;
                        lastRef[3] = sample;
                        break;

                    case 2:
                        lastRef[0] = lastRef[2];
                        lastRef[1] = (lastRef[0] + sample)/2;
                        lastRef[2] = sample;
                        break;

                    case 1:
                        lastRef[0] = sample;
                        break;
                    }
                    cycle = 0;
                }
                sample = iirB = iirB*(1-lowpass) + lastRef[cycle]*lowpass;
                sample = sample*wet + drySample*(1-wet);
                outputSampleL = dither(sample.channel[0], fpd[0]);
                outputSampleR = dither(sample.channel[1], fpd[1]);
            }
        };
    }
}
