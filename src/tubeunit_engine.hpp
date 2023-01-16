#ifndef __COSINEKITTY_TUBEUNIT_ENGINE_HPP
#define __COSINEKITTY_TUBEUNIT_ENGINE_HPP

// Sapphire tubular waveguide synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <complex>

#include "sapphire_engine.hpp"

namespace Sapphire
{
    using complex_t = std::complex<float>;

    class TubeUnitEngine
    {
    private:
        float sampleRate = 0.0f;

        bool dirty;                     // Do the delay lines need to be reconfigured?
        DelayLine<complex_t> outbound;  // sends pressure waves from the mouth to the opening
        DelayLine<complex_t> inbound;   // reflects pressure waves from the opening back to the mouth
        float airflow;                  // mass flow rate of air, normalized to [-1, +1].
        float rootFrequency;            // resonant frequency of tube in Hz
        complex_t mouthPressure;            // mouth chamber pressure relative to ambient atmosphere (i.e. can be negative) [Pa]
        float mouthVolume;              // total volume of mouth chamber (scales pressure changes relative to airflow rate) [m^3]
        float stopper1;                 // x-coordinate that limits leftward  movement of the piston [millimeters]
        float stopper2;                 // x-coordinate that limits rightward movement of the piston [millimeters]
        float bypass1;                  // x-coordinate where bypass starts to open [millimeters]
        float bypass2;                  // x-coordinate where bypass is fully open [millimeters]
        float bypassResistance;         // how much the bypass constricts airflow due to pressure differences across it
        float pistonPosition;           // x-coordinate of the piston [millimeters]
        float pistonSpeed;              // horizontal speed of the piston [millimeters/second]
        float pistonArea;               // cross-sectional surface area of the piston in m^2
        float pistonMass;               // mass of the piston [kg]
        float springRestLength;         // x-coordinate where spring is completely relaxed (zero force) [millimeters]
        float springConstant;           // converts displacement from spring rest position [mm] into newtons of force
        complex_t reflectionFraction;       // what fraction of pressure is reflected from the open end of the tube?
        float outputScale;              // divisor to limit output amplitude

        void configure()
        {
            if (sampleRate <= 0.0f)
                throw std::logic_error("Invalid sample rate in TubeUnitEngine");

            if (rootFrequency <= 0.0f)
                throw std::logic_error("Invalid root frequency in TubeUnitEngine");

            // A tube that is open on one end and closed on the other end has a negative
            // reflection at the open end and a positive reflection at the closed end.
            // Therefore a pulse has to travel the length of the tube 4 times
            // (that is, back and forth, then back and forth again) to complete a cycle.
            // Thus the tube must be 1/4 the length of the number of samples for a period of the root frequency.

            // FIXFIXFIX: Implement fractional sample wavelength using sinc formula.
            // For now, round down to integer number of samples.
            // Divide wavelength by 2 because we have both inbound and outbound delay lines.
            // But we can still get to the nearest integer sample by allowing one delay
            // line to have an odd length and the other an even length, if needed.
            // FIXFIXFIX ??? Do we really need two delay lines, or can we merge into one delay line?

            size_t nsamples = static_cast<size_t>(std::floor(sampleRate / (2.0 * rootFrequency)));
            size_t smallerHalf = nsamples / 2;
            size_t largerHalf = nsamples - smallerHalf;

            // The `setLength` calls will clamp the delay line lengths as needed.
            outbound.setLength(smallerHalf);
            inbound.setLength(largerHalf);
        }

    public:
        TubeUnitEngine()
        {
            initialize();
        }

        void initialize()
        {
            dirty = true;       // force re-configure
            outbound.clear();
            inbound.clear();
            airflow = 0.0f;
            rootFrequency = 3.0f;
            mouthPressure = 0.0f;
            mouthVolume = 3.0e-6;
            stopper1 = -10.0f;
            stopper2 = +10.0f;
            bypass1  =  +7.0f;
            bypass2  =  +8.0f;
            bypassResistance = 0.1f;
            pistonPosition = 0.0f;
            pistonSpeed = 0.0f;
            pistonArea = 6.45e-4;       // one square inch, converted to m^2
            pistonMass = 1.0e-5;        // 0.1 grams, converted to kg
            springRestLength = -1.0f;
            springConstant = 0.503f;
            reflectionFraction = complex_t(0.3f, -0.9f);
            outputScale = 100.0f;
        }

        void setSampleRate(float sampleRateHz)
        {
            sampleRate = sampleRateHz;
            dirty = true;
        }

        void setRootFrequency(float rootFrequencyHz)
        {
            rootFrequency = Clamp(rootFrequencyHz, 1.0f, 10000.0f);
            dirty = true;
        }

        float getRootFrequency() const
        {
            return rootFrequency;
        }

        void setAirflow(float airflowMassRate)
        {
            airflow = Clamp(airflowMassRate, -1.0f, +1.0f);
        }

        float getAirFlow() const
        {
            return airflow;
        }

        void setSpringConstant(float k)
        {
            springConstant = Clamp(k, 1.0e-6f, 1.0e+6f);
        }

        float getSpringConstant() const
        {
            return springConstant;
        }

        void setReflection(float real, float imag)
        {
            reflectionFraction = complex_t(real, imag);
        }

        complex_t getReflection() const
        {
            return reflectionFraction;
        }

        void process(float& leftOutput, float& rightOutput)
        {
            if (dirty)
            {
                configure();
                dirty = false;
            }

            // The tube has two ends: the breech and the bell.
            // The breech is where air enters from the mouth, around the piston, through the bypass.
            // The bell is where air exits at the end of the tube.
            complex_t breechPressure = inbound.readForward(0);

            // Use the piston's current position to determine whether,
            // and how much, the bypass valve is open.
            float bypassFraction = Clamp((pistonPosition - bypass1)/(bypass2 - bypass1));

            // The flow rate through the bypass is proportional to the pressure difference
            // across it, multiplied by the fraction it is currently open.
            complex_t bypassFlowRate = bypassFraction * bypassResistance * (mouthPressure - breechPressure);

            complex_t outSignal = breechPressure + bypassFlowRate;

            // Find the effective pressure the open end of the tube (the "bell"):
            complex_t bellPressure = outbound.readForward(0);

            // Keep vibrations moving through the two waveguides (delay lines).
            outbound.write(outSignal);

            // Reflection from the open end of a tube causes the return pressure
            // wave to be inverted.
            inbound.write(-reflectionFraction * bellPressure);

            // Update the pressure in the mouth by adding inbound airflow and subtracting outbound airflow.
            mouthPressure += (airflow - bypassFlowRate) / (mouthVolume * sampleRate);

#if 0
            // Do not allow mouth pressure to go below vacuum level.
            if (mouthPressure < -TubeUnitAtmosphericPressurePa)
                mouthPressure = -TubeUnitAtmosphericPressurePa;
#endif

            // Update the piston's position and speed using F=ma,
            // where F = ((net pressure) * area) - (spring force).
            complex_t netPistonForce = (mouthPressure - breechPressure)*pistonArea;
            netPistonForce -= (pistonPosition - springRestLength)*springConstant;

            // F = m*(dv/dt) ==> dv = (F/m)*dt = (F/m)/sampleRate
            complex_t dv = (netPistonForce / pistonMass) / sampleRate;

            // dx/dt = v ==> dx = v*dt = v/sampleRate
            // Use the mean speed over the interval.
            pistonPosition += (pistonSpeed + dv.real()/2) / sampleRate;

            // If the piston hits a stopper, halt its speed also.
            if (pistonPosition < stopper1)
            {
                pistonPosition = stopper1;
                pistonSpeed = 0.0f;
            }
            else if (pistonPosition > stopper2)
            {
                pistonPosition = stopper2;
                pistonSpeed = 0.0f;
            }
            else
            {
                pistonSpeed += dv.real();
            }

            leftOutput  = bellPressure.real() / outputScale;
            rightOutput = bellPressure.imag() / outputScale;
        }
    };
}

#endif // __COSINEKITTY_TUBEUNIT_ENGINE_HPP
