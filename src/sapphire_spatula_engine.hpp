#pragma once
#include <algorithm>
#include <cassert>
#include <complex>
#include <memory>
#include <vector>
#include "sapphire_granular_processor.hpp"

namespace Sapphire
{
    namespace Spatula
    {
        using Complex = std::complex<float>;

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
                    if (1 & (peakIndex - valleyIndex))
                        throw std::logic_error("peakIndex and valleyIndex must have an even difference.");

                    int delta = (valleyIndex > peakIndex) ? +2 : -2;
                    for (int index = peakIndex; index != valleyIndex + delta; index += delta)
                    {
                        // curve(peakIndex) = 1, curve(valleyIndex) = 0.
                        // when x=peakIndex, angle=0; when x=valleyIndex, angle=pi
                        float fraction = static_cast<float>(index - peakIndex) / (valleyIndex - peakIndex);
                        curve.at(index) = curve.at(index+1) = (1 + std::cos(M_PI * fraction)) / 2;
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


        class DispersionBuffer
        {
        private:
            float dispersionAngle = 0;
            std::vector<Complex> factorList;

        public:
            DispersionBuffer(int spectrumLength)
            {
                factorList.resize(spectrumLength);
                setStandardDeviationAngle(0);
            }

            void setStandardDeviationAngle(float dispersion)
            {
                if (dispersionAngle != dispersion)
                {
                    dispersionAngle = dispersion;
                    RandomVectorGenerator r;
                    for (Complex& factor : factorList)
                    {
                        float angle = dispersion * r.next();
                        factor = Complex(std::cos(angle), std::sin(angle));
                    }
                }
            }

            Complex getFactor(int spectrumIndex) const
            {
                return factorList.at(spectrumIndex);
            }
        };


        struct Band
        {
            SpectrumWindow window;
            DispersionBuffer dispersion;
            float amplitude = 1;

            Band(int blockSize, float freqLo, float freqCenter, float freqHi)
                : window(blockSize, freqLo, freqCenter, freqHi)
                , dispersion(blockSize / 2)     // there are half as many complex frequency values as real samples
                {}
        };


        class BandMixer : public FourierFilter
        {
        private:
            std::vector<Band> bandList;

            void addFrequencyBand(float freqLo, float freqCenter, float freqHi)
            {
                const int blockSize = getBlockSize();
                bandList.push_back(Band(blockSize, freqLo, freqCenter, freqHi));
            }

        public:
            explicit BandMixer(int _blockExponent)
                : FourierFilter(_blockExponent)
            {
                const float R = std::sqrt(10.0f);

                bandList.reserve(5);
                addFrequencyBand(0, 100, 100*R);
                addFrequencyBand(100, 100*R, 1000);
                addFrequencyBand(100*R, 1000, 1000*R);
                addFrequencyBand(1000, 1000*R, 10000);
                addFrequencyBand(1000*R, 10000, 20000);
            }

            int getBandCount() const
            {
                return static_cast<int>(bandList.size());
            }

            Band& band(int b)
            {
                return bandList.at(b);
            }

            void onSpectrum(float sampleRateHz, int length, const float* inSpectrum, float* outSpectrum) override
            {
                for (int index = 0; index < length; ++index)
                    outSpectrum[index] = 0;

                for (const Band& band : bandList)
                {
                    int indexLo, indexHi;
                    band.window.getIndexRange(indexLo, indexHi);
                    for (int index = indexLo; index <= indexHi; index += 2)
                    {
                        float k = band.amplitude * band.window.getCurve(index);
                        Complex z(k * inSpectrum[index+0], k * inSpectrum[index+1]);
                        z *= band.dispersion.getFactor(index / 2);
                        outSpectrum[index+0] += z.real();
                        outSpectrum[index+1] += z.imag();
                    }
                }
            }

            void setSampleRate(float sampleRateHz)
            {
                for (Band& band : bandList)
                    band.window.setSampleRate(sampleRateHz);
            }
        };


        class ChannelProcessor : public SingleChannelProcessor<float>       // create one per polyphonic input/output channel
        {
        private:
            BandMixer filter;
            FourierProcessor granulizer;
            const int blockExponent = 14;
            const int blockSize = 1 << blockExponent;

        public:
            explicit ChannelProcessor(int _blockExponent)
                : filter(_blockExponent)
                , granulizer(filter)
            {
            }

            void initialize() override
            {
                granulizer.initialize();
            }

            void setSampleRate(float sampleRateHz) override
            {
                filter.setSampleRate(sampleRateHz);
            }

            float process(float sampleRateHz, float input) override
            {
                return granulizer.process(sampleRateHz, input);
            }

            BandMixer& getBandMixer()
            {
                return filter;
            }
        };


        class FrameProcessor : public MultiChannelProcessor<float>
        {
        private:
            std::unique_ptr<ChannelProcessor> channelProcArray[MaxFrameChannels];

        public:
            using frame_t = Frame<float>;

            explicit FrameProcessor(int _blockExponent)
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                    channelProcArray[c] = std::make_unique<ChannelProcessor>(_blockExponent);
            }

            void initialize() override
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                    channelProcArray[c]->initialize();
            }

            void setSampleRate(float sampleRateHz) override
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                    channelProcArray[c]->setSampleRate(sampleRateHz);
            }

            void process(float sampleRateHz, const frame_t& inFrame, frame_t& outFrame) override
            {
                inFrame.validate();
                outFrame.length = inFrame.length;
                for (int c = 0; c < inFrame.length; ++c)
                    outFrame.data[c] = channelProcArray[c]->process(sampleRateHz, inFrame.data[c]);
            }

            BandMixer& mixer(int c)
            {
                if (c < 0 || c >= MaxFrameChannels)
                    throw std::invalid_argument("Channel index is invalid.");

                return channelProcArray[c]->getBandMixer();
            }

            void setBandAmplitude(int band, float amp)
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                    channelProcArray[c]->getBandMixer().band(band).amplitude = amp;
            }

            void setBandDispersion(int bandIndex, float dispersionStandardDeviationDegrees)
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                {
                    Band& band = channelProcArray[c]->getBandMixer().band(bandIndex);
                    band.dispersion.setStandardDeviationAngle(dispersionStandardDeviationDegrees);
                }
            }
        };
    }
}
