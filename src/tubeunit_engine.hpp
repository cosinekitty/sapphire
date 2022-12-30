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
        float rootFrequency;
        float formantFrequency;
        bool dirty;                 // Do the delay lines need to be reconfigured?
        bool active;                // Is the voice actively making sound?
        float voiceOnThreshold;     // the pressure below which we trigger a voice burst
        float voicePressure;
        int burstCounter;
        int burstDirection;
        int burstCycle = 0;

        void configure()
        {
            if (sampleRate <= 0.0f)
                throw std::logic_error("Invalid sample rate in TubeUnitEngine");

            if (rootFrequency <= 0.0f)
                throw std::logic_error("Invalid root frequency in TubeUnitEngine");

            if (formantFrequency <= 0.0f)
                throw std::logic_error("Invalid formant frequency in TubeUnitEngine");

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

            // The burst cycle length is the number of samples we send pressure for when the voice unit is active.
            burstCycle = std::max(1, static_cast<int>(std::floor(sampleRate / formantFrequency)));
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
            formantFrequency = TubeUnitDefaultFormantFrequencyHz;
            voiceOnThreshold = 0.1f;
            voicePressure = 0.0f;
            burstCounter = 0;
            burstDirection = 0;
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

            // Find the effective pressure the open end of the tube.
            float outSignal = inbound.readBackward(0) + outbound.readForward(0);

            voicePressure = 0.0f;

            if (active)
            {
                // The voice gate is high. Keep the formant vibrating.
                // When the pressure is below a certain threshold, trigger a burst of higher pressure.
                if (inSignal < voiceOnThreshold && burstDirection == 0)
                {
                    burstCounter = 0;
                    burstDirection = +1;
                }

                if (burstDirection != 0)
                {
                    burstCounter += burstDirection;
                    if (burstDirection > 0)
                    {
                        if (burstCounter >= burstCycle)
                            burstDirection = -1;    // Start moving the opposite way
                    }
                    else if (burstDirection < 0)
                    {
                        if (burstCounter <= 0)
                            burstDirection = 0;     // Come to a halt
                    }

                    voicePressure = static_cast<float>(burstCounter) / static_cast<float>(burstCycle);
                }
            }
            else
            {
                burstDirection = 0;
            }

            float reflectionPressure = 0.5 * outSignal;    // FIXFIXFIX - not a real formula

            // Even if the voice gate is low (not active), there can be a fadeout period.
            // Keep virbrations moving through the two waveguides (delay lines).
            outbound.write(voicePressure);
            inbound.write(reflectionPressure);
        }
    };
}

#endif // __COSINEKITTY_TUBEUNIT_ENGINE_HPP
