#ifndef __COSINEKITTY_TUBEUNIT_ENGINE_HPP
#define __COSINEKITTY_TUBEUNIT_ENGINE_HPP

// Sapphire tubular waveguide synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"

namespace Sapphire
{
    const float TubeUnitDefaultRootFrequencyHz    =  32.70319566257483f;    // C1
    const float TubeUnitDefaultFormantFrequencyHz = 130.8127826502993f;     // C3

    class TubeUnitEngine
    {
    private:
        DelayLine<float> outbound;
        DelayLine<float> inbound;
        float sampleRate = 0.0f;
        float rootFrequency = 0.0f;
        float formantFrequency = 0.0f;
        bool dirty = true;      // Do the delay lines need to be reconfigured?
        bool active = true;     // Is the voice actively making sound?

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
            // Consider a single delay line, but expanded to support multiple "taps".

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
            dirty = true;       // force re-configure
            active = true;      // voice is active by default
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

        void setFormantFrequency(float formantFrequencyHz)
        {
            formantFrequency = formantFrequencyHz;
            dirty = true;
        }

        void setActive(bool isVoiceActive)
        {
            active = isVoiceActive;
        }

        bool getActive() const
        {
            return active;
        }

        void process(float& leftOutput, float& rightOutput)
        {
            if (dirty)
            {
                configure();
                dirty = false;
            }

            leftOutput = 0.0f;
            rightOutput = 0.0f;
        }
    };
}

#endif // __COSINEKITTY_TUBEUNIT_ENGINE_HPP
