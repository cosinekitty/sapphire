#pragma once
#include <cassert>
#include <algorithm>
#include <vector>
#include "sapphire_granular_processor.hpp"

namespace Sapphire
{
    namespace Spatula
    {
        // We must have 4 samples in the block to have a DC component and a Nyquist
        // component that are distinct, while allowing even pairs (real, imag)
        // in the resulting FFT spectrum.
        const int MinBlockSize = 4;

        // Require blocks to be reasonably small, for use in realtime audio systems like VCV Rack.
        const int MaxBlockSize = (1 << 20);     // 1 MB

        inline bool IsValidBlockSize(int b)
        {
            return (b >= MinBlockSize) && (b <= MaxBlockSize) && ((b & 1) == 0);
        }

        inline int ValidateBlockSize(int b)
        {
            using namespace std;

            if (!IsValidBlockSize(b))
                throw invalid_argument(string("Invalid block size for Spatula: ") + to_string(b));

            return b;
        }

        class SpectrumWindow
        {
        private:
            const float freqLoHz;
            const float freqCenterHz;
            const float freqHiHz;
            const int blockSize;
            std::vector<float> curve;
            float sampleRate = 0;
            int indexLo = 0;
            int indexHi = -1;

            void halfCurve(int peakIndex, int valleyIndex)
            {
                if (valleyIndex == peakIndex)
                {
                    // Avoid division by zero.
                    curve.at(peakIndex) = 0;
                }
                else
                {
                    int delta = (valleyIndex > peakIndex) ? +1 : -1;
                    for (int index = peakIndex; index != valleyIndex + delta; index += delta)
                    {
                        // curve(peakIndex) = 1, curve(valleyIndex) = 0.
                        // when x=peakIndex, angle=0; when x=valleyIndex, angle=pi
                        float fraction = static_cast<float>(index - peakIndex) / (valleyIndex - peakIndex);
                        curve.at(index) = (1 + std::cos(M_PI * fraction)) / 2;
                    }
                }
            }

        public:
            explicit SpectrumWindow(int _blockSize, float _freqLoHz, float _freqCenterHz, float _freqHiHz)
                : freqLoHz(_freqLoHz)
                , freqCenterHz(_freqCenterHz)
                , freqHiHz(_freqHiHz)
                , blockSize(ValidateBlockSize(_blockSize))
            {
                curve.resize(blockSize);
            }

            int getBlockSize() const
            {
                return blockSize;
            }

            int indexForFrequency(float freqHz) const
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

                    indexLo = indexForFrequency(freqLoHz);
                    const int indexCenter = indexForFrequency(freqCenterHz);
                    indexHi = indexForFrequency(freqHiHz);

                    // Calculate two cosine curves, but with different frequencies
                    // to match the different width frequency ranges [lo, center] and [center, hi].
                    halfCurve(indexCenter, indexLo);
                    halfCurve(indexCenter, indexHi);
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
