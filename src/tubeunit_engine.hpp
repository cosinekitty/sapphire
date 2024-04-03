#pragma once

// Sapphire tubular waveguide synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <complex>
#include <cstddef>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    const float TubeUnitDefaultRootFrequencyHz = 3.0f;

    class TubeUnitEngine
    {
    private:
        float sampleRate = 0.0f;
        bool isQuiet;
        DelayLine<complex_t> outbound;  // sends pressure waves from the mouth to the opening
        DelayLine<complex_t> inbound;   // reflects pressure waves from the opening back to the mouth
        float airflow;                  // mass flow rate of air, normalized to [-1, +1].
        float rootFrequency;            // resonant frequency of tube in Hz
        complex_t mouthPressure;        // mouth chamber pressure relative to ambient atmosphere (i.e. can be negative) [Pa]
        float mouthVolume;              // total volume of mouth chamber (scales pressure changes relative to airflow rate) [m^3]
        float stopper1;                 // x-coordinate that limits leftward  movement of the piston [millimeters]
        float stopper2;                 // x-coordinate that limits rightward movement of the piston [millimeters]
        float bypass1;                  // x-coordinate where bypass starts to open [millimeters]
        float bypass2;                  // x-coordinate where bypass is fully open [millimeters]
        float bypassResistance;         // how much the bypass constricts airflow due to pressure differences across it
        complex_t pistonPosition;       // x-coordinate of the piston [millimeters]
        complex_t pistonSpeed;          // horizontal speed of the piston [millimeters/second]
        float pistonArea;               // cross-sectional surface area of the piston in m^2
        float pistonMass;               // mass of the piston [kg]
        float springRestLength;         // x-coordinate where spring is completely relaxed (zero force) [millimeters]
        float springConstant;           // converts displacement from spring rest position [mm] into newtons of force
        float reflectionDecay;          // how quickly reverberations die down in the tube [0, 1]
        float reflectionAngle;          // complex angle of reflection coefficient [fraction of pi]
        AutomaticGainLimiter agc;       // dynamically adapts to, and compensates for, excessive output levels
        bool enableAgc = false;
        float gain;
        float vortex;
        StagedFilter<complex_t, 1> dcRejectFilter;
        StagedFilter<complex_t, 1> loPassFilter;
        static const int windowSteps = 5;
        Interpolator<complex_t, windowSteps> interp;

    public:
        TubeUnitEngine()
        {
            initialize();
        }

        void initialize()
        {
            isQuiet = false;
            outbound.clear();
            inbound.clear();
            airflow = 0.0f;
            rootFrequency = TubeUnitDefaultRootFrequencyHz;
            mouthPressure = 0.0f;
            mouthVolume = 3.0e-6;
            stopper1 = -10.0f;
            stopper2 = +10.0f;
            bypass1  =  +7.0f;
            bypass2  =  +8.0f;
            bypassResistance = 0.1f;
            pistonPosition = {};
            pistonSpeed = {};
            pistonArea = 6.45e-4;       // one square inch, converted to m^2
            pistonMass = 1.0e-5;        // 0.1 grams, converted to kg
            springRestLength = -1.0f;
            springConstant = 0.503f;
            reflectionDecay = 0.5f;
            reflectionAngle = 0.87f;
            setAgcEnabled(true);
            setGain();
            vortex = 0.0f;
            dcRejectFilter.SetCutoffFrequency(10.0f);
            dcRejectFilter.Reset();
            loPassFilter.SetCutoffFrequency(8000.0f);
            loPassFilter.Reset();
        }

        bool getQuiet() const
        {
            return isQuiet;
        }

        void setQuiet(bool q)
        {
            isQuiet = q;
        }

        void setSampleRate(float sampleRateHz)
        {
            sampleRate = sampleRateHz;
        }

        void setRootFrequency(float rootFrequencyHz)
        {
            rootFrequency = std::clamp(rootFrequencyHz, 1.0f, 10000.0f);
        }

        float getRootFrequency() const
        {
            return rootFrequency;
        }

        void setAirflow(float airflowMassRate)
        {
            airflow = std::clamp(airflowMassRate, -1.0f, +10.0f);
        }

        float getAirFlow() const
        {
            return airflow;
        }

        void setSpringConstant(float k)
        {
            springConstant = std::clamp(k, 1.0e-6f, 1.0e+6f);
        }

        float getSpringConstant() const
        {
            return springConstant;
        }

        void setReflectionDecay(float decay)
        {
            reflectionDecay = decay;
        }

        void setReflectionAngle(float angle)
        {
            reflectionAngle = angle;
        }

        bool getAgcEnabled() const
        {
            return enableAgc;
        }

        void setAgcEnabled(bool enable)
        {
            if (enable && !enableAgc)
            {
                // If the AGC isn't enabled, and caller wants to enable it,
                // re-initialize the AGC so it forgets any previous level it had settled on.
                agc.initialize();
            }
            enableAgc = enable;
        }

        void setAgcLevel(float level)
        {
            // Unlike Elastika, Tube Unit is calibrated for unit dimensionless output.
            // The `level` value must be compensated by the caller the same way that
            // the output from `process` is used.
            agc.setCeiling(level);
        }

        double getAgcDistortion() const     // returns 0 when no distortion, or a positive value correlated with AGC distortion
        {
            return enableAgc ? (agc.getFollower() - 1.0) : 0.0;
        }

        void setGain(float slider = 1.0f)       // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
        {
            gain = std::pow(std::clamp(slider, 0.0f, 2.0f), 4.0f) / 80.0f;
        }

        float getBypassWidth() const
        {
            return bypass2 - bypass1;
        }

        float getBypassCenter() const
        {
            return (bypass1 + bypass2) / 2;
        }

        void setBypassWidth(float width)
        {
            // Dilate around the current center.
            float center = getBypassCenter();
            float dilate = std::clamp(width/2, 0.01f, stopper2 - stopper1);
            bypass1 = center - dilate;
            bypass2 = center + dilate;
        }

        void setBypassCenter(float center)
        {
            float dilate = getBypassWidth() / 2;
            float clampedCenter = std::clamp(center, stopper1, stopper2);
            bypass1 = clampedCenter - dilate;
            bypass2 = clampedCenter + dilate;
        }

        void setVortex(float v)
        {
            vortex = v;
        }

        void process(float& leftOutput, float& rightOutput, float leftInput, float rightInput)
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

            // Divide wavelength by 2 because we have both inbound and outbound delay lines.
            // Add extra samples needed for the interpolator window, and round up to next higher integer.
            double roundTripSamples = (sampleRate / (2.0 * rootFrequency));

            std::size_t nsamples = static_cast<std::size_t>(std::floor(roundTripSamples));
            std::size_t smallerHalf = nsamples / 2;
            std::size_t largerHalf = nsamples - smallerHalf;

            if (largerHalf < windowSteps + 1)
                throw std::logic_error("outbound delay line is not large enough for interpolation.");

            outbound.setLength(largerHalf + windowSteps);
            inbound.setLength(smallerHalf);

            // Copy the window of outbound samples into a sinc-interpolator.
            for (int n = -windowSteps; n <= +windowSteps; ++n)
                interp.write(n, outbound.readForward(n + windowSteps));

            // Find the effective pressure the open end of the tube (the "bell").
            // Use the interpolator to handle the fractional number of samples needed
            // to produce the exact root frequency.

            complex_t bellPressure = interp.read(nsamples - roundTripSamples);
            bellPressure = dcRejectFilter.UpdateHiPass(bellPressure, sampleRate);

            // The tube has two ends: the breech and the bell.
            // The breech is where air enters from the mouth, around the piston, through the bypass.
            // The bell is where air exits at the end of the tube.
            complex_t breechPressure = inbound.readForward(0);

            // Use the piston's current position to determine whether,
            // and how much, the bypass valve is open.
            float bypassFraction = std::clamp((pistonPosition.real() - bypass1)/(bypass2 - bypass1), 0.0f, 1.0f);

            // The flow rate through the bypass is proportional to the pressure difference
            // across it, multiplied by the fraction it is currently open.
            complex_t bypassFlowRate = bypassFraction * bypassResistance * (mouthPressure - breechPressure);

            complex_t outSignal = breechPressure + bypassFlowRate;
            if (!isQuiet)
                outSignal += 5.0f*complex_t{leftInput, rightInput};

            // Keep vibrations moving through the two waveguides (delay lines).
            outbound.write(outSignal);

            // Reflection from the open end of a tube causes the return pressure
            // wave to be inverted.
            // Convert the (decay, angle) pair into a complex coefficient.
            float halflife = std::pow(10.0f, (2.0 * reflectionDecay) - 1.0);     // exponential range 0.1 seconds ... 10 seconds.
            float magnitude = std::pow(0.5f, static_cast<float>(1.0 / (rootFrequency * halflife)));
            float radians = M_PI * reflectionAngle;
            complex_t reflectionFraction { magnitude * std::cos(radians), magnitude * std::sin(radians) };
            inbound.write(-reflectionFraction * bellPressure);

            if (isQuiet)
            {
                // Immediately vent all mouth pressure and ignore all airflow.
                mouthPressure = {};
            }
            else
            {
                // Update the pressure in the mouth by adding inbound airflow and subtracting outbound airflow.
                mouthPressure += (airflow - bypassFlowRate) / (mouthVolume * sampleRate);
            }

            // Update the piston's position and speed using F=ma,
            // where F = ((net pressure) * area) - (spring force).
            // Then from force, calculate velocity increment:
            // F = m*(dv/dt) ==> dv = (F/m)*dt = (F/m)/sampleRate
            complex_t dv = (
                (mouthPressure - breechPressure)*pistonArea -
                (pistonPosition - springRestLength)*springConstant
            ) / (pistonMass * sampleRate);

            // Include a little weirdness using the `vortex` parameter.
            float dvmag = std::abs(dv);
            if (dvmag > 0.0f)       // prevent division by zero; if dv=0, leave it 0.
            {
                float x = vortex / 2;
                complex_t dir = dv / dvmag;
                dv *= ((1-x) + x*dir);
            }

            // dx/dt = v ==> dx = v*dt = v/sampleRate
            // Use the mean speed over the interval.
            pistonPosition += (pistonSpeed + dv/2.0f) / sampleRate;

            // If the piston hits a stopper, halt its speed also.
            if (pistonPosition.real() < stopper1)
            {
                pistonPosition = stopper1;
                pistonSpeed = {};
            }
            else if (pistonPosition.real() > stopper2)
            {
                pistonPosition = stopper2;
                pistonSpeed = {};
            }
            else
            {
                pistonSpeed += dv;
            }

            complex_t result = loPassFilter.UpdateLoPass(bellPressure * complex_t{1,1}, sampleRate);
            leftOutput  = result.real() * gain;
            rightOutput = result.imag() * gain;

            if (enableAgc)
            {
                // Automatic gain control to limit excessive output voltages.
                agc.process(sampleRate, leftOutput, rightOutput);
            }
        }
    };
}
