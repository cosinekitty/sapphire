#pragma once
#include <algorithm>
#include <vector>
#include "sapphire_granular_processor.hpp"

namespace Sapphire
{
    namespace Spatula
    {
        class SpectrumWindow
        {
        private:
            const float freqLoHz;
            const float freqCenterHz;
            const float freqHiHz;
            std::vector<float> curve;
            float sampleRate = 0;
            int indexLo = 0;
            int indexHi = -1;

        public:
            explicit SpectrumWindow(int _blockSize, float _freqLoHz, float _freqCenterHz, float _freqHiHz)
                : freqLoHz(_freqLoHz)
                , freqCenterHz(_freqCenterHz)
                , freqHiHz(_freqHiHz)
            {
                curve.resize(_blockSize);
            }

            int getBlockSize() const
            {
                return static_cast<int>(curve.size());
            }

            int indexForFrequency(float freqHz)
            {
                // The FFT outputs a complex number (real, imag) for each frequency up to the Nyquist frequency.
                // spectrum[0], spectrum[1] ==> 0 Hz
                // spectrum[blockSize-2], spectrum[blockSize-1] ==> (samplingRate/2) Hz
                // There are blockSize/2 complex numbers in the spectrum, with a maximum
                // frequency of samplingRate/2. Dividing the two gives samplingRate/blockSize Hz
                // per complex pair.
                const float hzPerSpectrumPair = sampleRate / getBlockSize();

                // Divide the desired frequency by the bandwidth of each spectrum pair
                // to obtain the spectrum pair index. Multiply by 2 to ensure landing
                // on the front of a (real, imag) pair.
                int index = 2 * static_cast<int>(std::round(freqHz / hzPerSpectrumPair));

                // Clamp to make sure we don't go outside valid memory bounds.
                index = std::max(0, std::min(getBlockSize()-2, index));
                return index;
            }

            void setSampleRate(float newSampleRate)
            {
                if (newSampleRate != sampleRate)
                {
                    // Must update the sample rate before calling indexForFrequency().
                    sampleRate = newSampleRate;

                    // Calculate two cosine curves, but with different frequencies
                    // to match the different width frequency ranges [lo, center] and [center, hi].

                    indexLo = indexForFrequency(freqLoHz);
                    //int indexCenter = indexForFrequency(freqCenterHz);
                    indexHi = indexForFrequency(freqHiHz);
                }
            }

            void getIndexRange(int& spectrumIndexLo, int& spectrumIndexHi) const
            {
                spectrumIndexLo = indexLo;
                spectrumIndexHi = indexHi;
            }

            float getCurve(int index) const
            {
                if (index < indexLo || index > indexHi)
                    return 0;

                return curve.at(index);
            }
        };
    }
}
