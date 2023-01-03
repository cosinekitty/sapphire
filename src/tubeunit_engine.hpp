#ifndef __COSINEKITTY_TUBEUNIT_ENGINE_HPP
#define __COSINEKITTY_TUBEUNIT_ENGINE_HPP

// Sapphire tubular waveguide synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"

namespace Sapphire
{
    const float TubeUnitDefaultRootFrequencyHz    =  32.70319566257483f;    // C1

    class TubeUnitEngine
    {
    private:
        DelayLine<float> outbound;
        DelayLine<float> inbound;
        float sampleRate = 0.0f;
        float airflow = 0.0f;           // mass flow rate of air, normalized to [-1, +1].
        float rootFrequency = 0.0f;     // resonant frequency of tube in Hz
        bool dirty = true;              // Do the delay lines need to be reconfigured?

        void configure()
        {
            if (sampleRate <= 0.0f)
                throw std::logic_error("Invalid sample rate in TubeUnitEngine");

            if (rootFrequency <= 0.0f)
                throw std::logic_error("Invalid root frequency in TubeUnitEngine");

            // FIXFIXFIX: Implement fractional sample wavelength using sinc formula.
            // For now, round down to integer number of samples.
            // Divide wavelength by 2 because we have both inbound and outbound delay lines.
            // But we can still get to the nearest integer sample by allowing one delay
            // line to have an odd length and the other an even length, if needed.
            // FIXFIXFIX ??? Do we really need two delay lines, or can we merge into one delay line?

            size_t nsamples = static_cast<size_t>(std::floor(sampleRate / rootFrequency));
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
            inbound.clear();
            outbound.clear();
            rootFrequency = TubeUnitDefaultRootFrequencyHz;
            airflow = 0.0f;
            dirty = true;       // force re-configure
        }

        void setSampleRate(float sampleRateHz)
        {
            sampleRate = sampleRateHz;
            dirty = true;
        }

        void setRootFrequency(float rootFrequencyHz)
        {
            // Clamp the root frequency to a range that is representable by the waveguides.
            rootFrequency = rootFrequencyHz;
            dirty = true;
        }

        void setAirflow(float airflowMassRate)
        {
            airflow = Clamp(airflowMassRate, -1.0f, +1.0f);
        }

        void process(float& leftOutput, float& rightOutput)
        {
            // Guarantee some kind of output: start with silence.
            leftOutput = rightOutput = 0.0f;

            if (dirty)
            {
                configure();
                dirty = false;
            }

            // The tube has two ends: an open end where the sound comes out
            // and a closed end where the "voice" is applied. The so-called
            // voice is analogous to a clarinet reed or a trumpet player's lips.
            // Find the effective air pressure at the closed end of the tube.
            float inSignal = inbound.readForward(0) + outbound.readBackward(0);
            (void)inSignal;

            // Find the effective pressure the open end of the tube.
            float outSignal = inbound.readBackward(0) + outbound.readForward(0);
            (void)outSignal;

            float voicePressure = 0.0f;

            float reflectionPressure = 0.5 * outSignal;    // FIXFIXFIX - not a real formula

            // Even if the voice gate is low (not active), there can be a fadeout period.
            // Keep virbrations moving through the two waveguides (delay lines).
            outbound.write(voicePressure);
            inbound.write(reflectionPressure);

            leftOutput = rightOutput = voicePressure;
        }
    };
}

#endif // __COSINEKITTY_TUBEUNIT_ENGINE_HPP
