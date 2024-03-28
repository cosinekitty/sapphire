#pragma once
#include <algorithm>
#include <cassert>
#include <complex>
#include <memory>
#include <stdexcept>
#include <string>
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
            if (!IsValidBlockSize(b))
                throw std::invalid_argument(std::string("Invalid block size for Spatula: ") + std::to_string(b));

            return b;
        }

        inline int ValidateIndex(int n, int i)
        {
            if (i < 0)
                throw std::invalid_argument("Index is not allowed to be negative.");

            if (i >= n)
                throw std::invalid_argument("Index is not allowed to go beyond the end of the array.");

            return i;
        }

        class SpectrumWindow
        {
        private:
            const float freqLoHz;
            const float freqCenterHz;
            const float freqHiHz;
            const int spectrumLength;
            std::vector<float> curve;    // indexed by [0..(blockSize/2)-1] : each spectrum line is a complex number
            float sampleRate = 0;
            int indexLo = 0;
            int indexHi = -1;

            void halfCurve(int peakIndex, int valleyIndex)
            {
                if (valleyIndex == peakIndex)
                {
                    // Avoid division by zero.
                    // Use mean value between [0, 1] to represent both endpoints as fairly as possible.
                    curve.at(peakIndex) = 0.5f;
                }
                else
                {
                    int delta = (valleyIndex > peakIndex) ? +1 : -1;
                    for (int index = peakIndex; index != valleyIndex + delta; index += delta)
                    {
                        ValidateIndex(spectrumLength, index);
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
                , spectrumLength(ValidateBlockSize(_blockSize) / 2)
            {
                curve.resize(spectrumLength);
            }

            int indexForFrequency(float freqHz) const
            {
                // The FFT outputs a complex number (real, imag) for each frequency up to the Nyquist frequency.
                // The `curve` array has one entry per complex number; therefore it has length blockSize/2.
                // spectrum[0] ==> 0 Hz
                // spectrum[spectrumLength-1] ==> (samplingRate/2) Hz
                const float hzPerSpectrumPair = sampleRate / (2 * spectrumLength);

                // Divide the desired frequency by the bandwidth of each spectrum pair
                // to obtain the spectrum pair index.
                int index = static_cast<int>(std::round(freqHz / hzPerSpectrumPair));

                // Clamp to make sure we don't go outside valid memory bounds.
                index = std::max(0, std::min(spectrumLength-1, index));

                return ValidateIndex(spectrumLength, index);
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
            float dispersionAngle = -1;     // force setStandardDeviationAngle(0) to work when called by ctor
            std::vector<Complex> factorList;

        public:
            explicit DispersionBuffer(int spectrumLength)
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
                    const float radians = (M_PI / 180) * dispersion;
                    for (Complex& factor : factorList)
                    {
                        float angle = radians * r.next();
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
            float pendingDispersionAngle{};
            bool isDispersionAngleReady = false;

            Band(int blockSize, float freqLo, float freqCenter, float freqHi)
                : window(blockSize, freqLo, freqCenter, freqHi)
                , dispersion(blockSize / 2)     // there are half as many complex frequency values as real samples
                {}

            void setPendingDispersion(float dispersionStandardDeviationDegrees)
            {
                pendingDispersionAngle = dispersionStandardDeviationDegrees;
                isDispersionAngleReady = true;
            }
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

                for (Band& b : bandList)
                {
                    if (b.isDispersionAngleReady)
                    {
                        b.dispersion.setStandardDeviationAngle(b.pendingDispersionAngle);
                        b.isDispersionAngleReady = false;
                    }

                    int spectrumIndexLo, spectrumIndexHi;
                    b.window.getIndexRange(spectrumIndexLo, spectrumIndexHi);
                    for (int spectrumIndex = spectrumIndexLo; spectrumIndex <= spectrumIndexHi; ++spectrumIndex)
                    {
                        int realIndex = 2*spectrumIndex;
                        int imagIndex = realIndex + 1;
                        float k = b.amplitude * b.window.getCurve(spectrumIndex);
                        Complex z(k * inSpectrum[realIndex], k * inSpectrum[imagIndex]);
                        z *= b.dispersion.getFactor(spectrumIndex);
                        outSpectrum[realIndex] += z.real();
                        outSpectrum[imagIndex] += z.imag();
                    }
                }
            }

            void setSampleRate(float sampleRateHz)
            {
                for (Band& b : bandList)
                    b.window.setSampleRate(sampleRateHz);
            }
        };


        class ChannelProcessor : public SingleChannelProcessor<float>       // create one per polyphonic input/output channel
        {
        private:
            BandMixer filter;
            FourierProcessor granulizer;

        public:
            explicit ChannelProcessor(int _blockExponent)
                : filter(_blockExponent)
                , granulizer(filter)
            {
            }

            void initialize() override
            {
                granulizer.initialize();    // will call filter.initialize() for us
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

            bool isFinalFrameBeforeBlockChange() const
            {
                // Return true if the next frame to be processed will cause another block to be processed.
                return granulizer.isFinalFrameBeforeBlockChange();
            }
        };


        class FrameProcessor : public MultiChannelProcessor<float>
        {
        private:
            float prevSampleRate = -1;
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

                // Force resetting sample rates in channelProcArray
                // the next time setSampleRate is called.
                prevSampleRate = -1;
            }

            bool isFinalFrameBeforeBlockChange() const
            {
                // Return true if the next frame to be processed will cause another block to be processed.
                // Assume that all channel processors stay in sync. Then the first channel processor
                // can answer the question for all channel processors.
                return channelProcArray[0] && channelProcArray[0]->isFinalFrameBeforeBlockChange();
            }

            void setSampleRate(float sampleRateHz) override
            {
                // We don't optimize for sampleRateHz==prevSampleRate
                // because there could be situations in which setting
                // the sampling rate has some important side effects.
                // This provides the opportunity for calling code to
                // trigger sample rate notifications as often as needed.

                for (int c = 0; c < MaxFrameChannels; ++c)
                    channelProcArray[c]->setSampleRate(sampleRateHz);

                // However, we do provide an opportunity to optimize when it makes sense.
                prevSampleRate = sampleRateHz;
            }

            void process(float sampleRateHz, const frame_t& inFrame, frame_t& outFrame) override
            {
                // Optimize for very frequent calls, but relieve calling code
                // from the burden of notifying us of changes to the sampling rate.
                if (sampleRateHz != prevSampleRate)
                    setSampleRate(sampleRateHz);

                // Run each channel of inFrame through its respective channel processor.
                // Store the resulting output channels in outFrame.
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
                    band.setPendingDispersion(dispersionStandardDeviationDegrees);
                }
            }
        };
    }
}
