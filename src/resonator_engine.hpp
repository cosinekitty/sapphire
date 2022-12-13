#ifndef __COSINEKITTY_RESONATOR_ENGINE_HPP
#define __COSINEKITTY_RESONATOR_ENGINE_HPP

#include "sapphire_engine.hpp"

namespace Sapphire
{
    struct QuadComplex
    {
        PhysicsVector real;
        PhysicsVector imag;

        QuadComplex(PhysicsVector _real, PhysicsVector _imag)
            : real(_real)
            , imag(_imag)
            {}

        QuadComplex(float _real, float _imag)
            : real(PhysicsVector(_real))
            , imag(PhysicsVector(_imag))
            {}
    };

    inline QuadComplex operator * (const QuadComplex& a, const QuadComplex& b)
    {
        return QuadComplex {
            a.real*b.real - a.imag*b.imag,
            a.real*b.imag + a.imag*b.real
        };
    }

    inline QuadComplex operator + (const QuadComplex& a, const QuadComplex& b)
    {
        return QuadComplex { a.real + b.real, a.imag + b.imag };
    }

    inline QuadComplex operator * (float x, const QuadComplex &c)
    {
        return QuadComplex { x * c.real, x * c.imag };
    }

    struct QuadPhasor
    {
        float alpha = 0.0001f;
        QuadComplex step_angle  { 1.0f, 0.0f };
        QuadComplex phase_angle { 1.0f, 0.0f };
        QuadComplex accum { 0.0f, 0.0f };

        void setFrequency(int index, float sampleRate, float frequencyHz)
        {
            if (index < 0 || index > 3)
                throw std::range_error("Phasor index must be 0..3.");

            float radiansPerSample = (2.0 * M_PI * frequencyHz) / sampleRate;
            step_angle.real[index] = std::cos(radiansPerSample);
            step_angle.imag[index] = std::sin(radiansPerSample);
        }

        void process(float inReal, float inImag, float &sumReal, float &sumImag)
        {
            // Update the phase angle by a sample's worth of angular change.
            phase_angle = phase_angle * step_angle;

            // Update the accumulators for each frequency.
            QuadComplex x {inReal, inImag};
            accum = alpha*x*phase_angle + (1-alpha)*accum;

            // Remodulate the accumulator to produce an output signal.
            QuadComplex y = accum * phase_angle;
            sumReal += y.real[0] + y.real[1] + y.real[2] + y.real[3];
            sumImag += y.imag[0] + y.imag[1] + y.imag[2] + y.imag[3];
        }
    };

    class ResonatorEngine
    {
    private:
        QuadPhasor quad[4];     // 4 QuadPhasors = 16 phasors for maximum polyphony

    public:
        ResonatorEngine(float sampleRate)
        {
            setSampleRate(sampleRate);
        }

        void setSampleRate(float sampleRate)
        {
            float frequencyHz = 220.0f;
            float step = std::pow(2.0, 0.25);
            for (int i = 0; i < 16; ++i)
            {
                quad[i/4].setFrequency(i%4, sampleRate, frequencyHz);
                frequencyHz *= step;
            }
        }

        void process(float leftIn, float rightIn, float& leftOut, float& rightOut)
        {
            leftOut = 0.0f;
            rightOut = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                quad[i].process(leftIn, rightIn, leftOut, rightOut);
            }
        }
    };
}

#endif // __COSINEKITTY_RESONATOR_ENGINE_HPP
