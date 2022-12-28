#ifndef __COSINEKITTY_TUBEUNIT_ENGINE_HPP
#define __COSINEKITTY_TUBEUNIT_ENGINE_HPP

// Sapphire tubular waveguide synth, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"

namespace Sapphire
{
    class DelayLine
    {
    private:
        std::vector<float> buffer;
        size_t front = 0;       // postion where data is inserted
        size_t back = 0;        // postion where data is removed

    public:
        static const size_t maxSamples = 10000;

        DelayLine()
        {
            buffer.resize(maxSamples);
        }

        float read() const
        {
            return buffer.at(back);
        }

        void write(float x)
        {
            buffer.at(front) = x;
            front = (front + 1) % maxSamples;
            back = (back + 1) % maxSamples;
        }

        size_t getLength() const
        {
            return 1 + (((maxSamples + front) - back) % maxSamples);
        }

        void setLength(size_t nsamples)
        {
            if (nsamples < 1 || nsamples > maxSamples)
                throw std::range_error(std::string("Delay line number of samples must be 1..") + std::to_string(static_cast<unsigned long>(maxSamples)));

            // Leave `front` where it is. Adjust `back` forward or backward as needed.
            // If `front` and `back` are the same, then the length is 1 sample,
            // because the usage contract is to to call read() before calling write().

            back = ((front + maxSamples) - nsamples) % maxSamples;
        }

        void clear()
        {
            for (float& x : buffer)
                x = 0.0f;
        }
    };


    class TubeUnitEngine
    {
    private:
        DelayLine outbound;
        DelayLine inbound;
        float sampleRate = 0.0f;
        float rootFrequency = 0.0f;
        bool dirty = true;      // Do the delay lines need to be reconfigured?

        void configure()
        {
            if (sampleRate <= 0.0f)
                throw std::logic_error("Invalid sample rate in TubeUnitEngine");

            if (rootFrequency <= 0.0f || rootFrequency >= sampleRate / 2.0f)
                throw std::logic_error("Invalid root frequency in TubeUnitEngine");
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
        }

        void setSampleRate(float sampleRateHz)
        {
            // FIXFIXFIX: Implement fractional sample wavelength using sinc formula.
            // For now, round down to integer number of samples.
            // Divide wavelength by 2 because we have both inbound and outbound delay lines.
            // But we can still get to the nearest integer sample by allowing one delay
            // line to have an odd length and the other an even length, if needed.
            // FIXFIXFIX ??? Do we really need two delay lines, or can we merge into one delay line?
            sampleRate = sampleRateHz;
            dirty = true;
        }

        void setRootFrequency(float rootFrequencyHz)
        {
            // Clamp the root frequency to a range that is representable by the waveguides.
            rootFrequency = rootFrequencyHz;
            dirty = true;
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
