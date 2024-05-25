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


        struct StereoQuad
        {
            StereoFrame frame[4];

            void clear()
            {
                for (int i = 0; i < 4; ++i)
                    frame[i].clear();
            }

            StereoFrame sum() const
            {
                return frame[0] + frame[1] + frame[2] + frame[3];
            }
        };


        struct DelayLine
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

            StereoFrame tailFrame()
            {
                return buffer.at(count - offset(count));
            }
        };


        struct Engine
        {
        private:
            // Structure
            const int tankSize[12] =
            {
                4801,
                2909,
                1153,
                 461,
                7607,
                4217,
                2269,
                1597,
                3407,
                1823,
                 859,
                 331
            };

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
            DelayLine delay[NDELAYS];
            StereoQuad feedback;
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

            static int validateTankIndex(int tankIndex)
            {
                if (tankIndex < 0 || tankIndex >= NDELAYS)
                    throw std::out_of_range(std::string("tankIndex is invalid: ") + std::to_string(tankIndex));
                return tankIndex;
            }

            DelayLine& dstate(int tankIndex)
            {
                validateTankIndex(tankIndex);
                return delay[tankIndex];
            }

            stereo_buffer_t& tank(int tankIndex)
            {
                validateTankIndex(tankIndex);
                return delay[tankIndex].buffer;
            }

            double& access(int channel, int tankIndex, int sampleIndex)
            {
                return tank(tankIndex).at(sampleIndex).channel[channel&1];
            }

            StereoFrame& headFrame(int tankIndex)
            {
                validateTankIndex(tankIndex);
                const int headIndex = delay[tankIndex].count;
                return delay[tankIndex].buffer.at(headIndex);
            }

            StereoFrame tailFrame(int tankIndex)
            {
                validateTankIndex(tankIndex);
                return delay[tankIndex].tailFrame();
            }

            StereoQuad read(int tankStartIndex)
            {
                StereoQuad f;
                f.frame[0] = tailFrame(tankStartIndex+0);
                f.frame[1] = tailFrame(tankStartIndex+1);
                f.frame[2] = tailFrame(tankStartIndex+2);
                f.frame[3] = tailFrame(tankStartIndex+3);
                return f;
            }

            void write(int tankIndex, const StereoFrame& frame)
            {
                headFrame(tankIndex) = frame;
                dstate(tankIndex).advance();
            }

            StereoQuad stir(const StereoQuad& f)
            {
                StereoQuad g;
                g.frame[0] = f.frame[0] - (f.frame[1] + f.frame[2] + f.frame[3]);
                g.frame[1] = f.frame[1] - (f.frame[0] + f.frame[2] + f.frame[3]);
                g.frame[2] = f.frame[2] - (f.frame[0] + f.frame[1] + f.frame[3]);
                g.frame[3] = f.frame[3] - (f.frame[0] + f.frame[1] + f.frame[2]);
                return g;
            }

            void write(int tankStartIndex, const StereoQuad& f)
            {
                StereoQuad g = stir(f);
                for (int i = 0; i < 4; ++i)
                    write(tankStartIndex + i, g.frame[i]);
            }

            void reflect(int tankStartIndex, const StereoFrame& sample, double regen)
            {
                for (int i = 0; i < 4; ++i)
                    write(tankStartIndex + i, sample + (feedback.frame[i].flip() * regen));
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

            void mix(const StereoFrame& sample)
            {
                switch (cycle)
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
                for (int i = 0; i <= MAXCYCLE; ++i)
                    lastRef[i].clear();
                feedback.clear();
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

                // Map knob values onto internal parameter values.
                const double regen = 0.0625+((1.0-replaceKnob)*0.0625);
                const double attenuate = (1.0 - (regen / 0.125))*1.333;
                const double lowpass = Square(1.00001-(1.0-brightKnob))/std::sqrt(overallscale);
                const double drift = Cube(detuneKnob)*0.001;
                const double size = (bignessKnob*1.77)+0.1;
                const double wet = 1-Cube(1 - mixKnob);

                // Update tank sizes as the bigness knob is adjusted.
                for (int i = 0; i < 12; ++i)
                    delay[i].delay = tankSize[i] * size;
                delay[12].delay = 256;

                // ??? Do not allow silence? Add a teensy bit of noise?
                if (std::abs(inputSampleL) < 1.18e-23) inputSampleL = fpd[0] * 1.18e-17;
                if (std::abs(inputSampleR) < 1.18e-23) inputSampleR = fpd[1] * 1.18e-17;
                const StereoFrame drySample(inputSampleL, inputSampleR);

                vibM += oldfpd * drift;
                if (vibM > 2*M_PI)
                {
                    vibM = 0;
                    oldfpd = 0.4294967295 + (fpd[0] * 0.0000000000618);
                }

                write(12, drySample * attenuate);

                StereoFrame phasor(interp(0, 0), interp(1, M_PI_2));
                StereoFrame sample = iirA = iirA*(1-lowpass) + phasor*lowpass;

                if (++cycle == cycleEnd)
                {
                    reflect(8, sample, regen);
                    write(0, read(8));
                    write(4, read(0));
                    StereoQuad f = read(4);
                    feedback = stir(f);
                    mix(f.sum() / 8);
                    cycle = 0;
                }
                iirB = iirB*(1-lowpass) + lastRef[cycle]*lowpass;
                sample = iirB*wet + drySample*(1-wet);
                outputSampleL = dither(sample.channel[0], fpd[0]);
                outputSampleR = dither(sample.channel[1], fpd[1]);
            }
        };
    }
}
