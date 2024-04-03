#pragma once
#include <algorithm>
#include <cassert>
#include <complex>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "sapphire_simd.hpp"
#include "sapphire_granular_processor.hpp"

namespace Sapphire
{
    namespace Bash
    {
        inline constexpr bool IsPowerOfTwo(int n)
        {
            return (n > 1) && (n & (n-1)) == 0;
        }

        // We must have 4 samples in the block to have a DC component and a Nyquist
        // component that are distinct, while allowing even pairs (real, imag)
        // in the resulting FFT spectrum.
        constexpr int MinBlockSize = 4;
        static_assert(IsPowerOfTwo(MinBlockSize), "Minimum block size must be 2**N, where N is a positive integer.");

        // Require blocks to be reasonably small, for use in realtime audio systems like VCV Rack.
        constexpr int MaxBlockSize = (1 << 20);     // 1 MB
        static_assert(IsPowerOfTwo(MaxBlockSize), "Maximum block size must be 2**N, where N is a positive integer.");

        inline constexpr bool IsValidBlockSize(int b)
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


        constexpr float DefaultCenterFrequencyHz = 523.25113;     // 440*(2**(3/12)) = middle C
        constexpr float MinBandwidth = 0.1;
        constexpr float MaxBandwidth = 4;
        static_assert(MinBandwidth < MaxBandwidth, "Bandwidth range must be in ascending order.");

        struct IndexRange
        {
            int lo;
            int hi;

            explicit IndexRange(int _lo, int _hi)
                : lo(_lo)
                , hi(_hi)
                {}
        };

        constexpr float OctaveHalfRange = 4;

        struct FrequencyBand
        {
            float lo;
            float center;
            float hi;

            explicit FrequencyBand(float _lo, float _center, float _hi)
                : lo(_hi)
                , center(_center)
                , hi(_hi)
                {}
        };

        class SpectrumWindow
        {
        private:
            const float freqLoHz;
            const float freqCenterHz;
            const float freqHiHz;
            const int spectrumLength;
            std::vector<float> curve;    // indexed by [0..(blockSize/2)-1] : each spectrum line is a complex number
            bool isCurveDirty = true;
            float sampleRate = 0;
            float bandwidth = 1;
            int indexLo = 0;
            int indexHi = -1;
            float frequencyOffsetOctaves = 0;
            float f1{};
            float f2{};
            float fc{};

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
                        curve.at(index) = (1 + std::cos(M_PI * fraction)) / 2;
                    }
                }
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
                // Multiply by the dimensionless parameter `bandwidth` to allow user-adjusted frequency bandwidths.
                int index = static_cast<int>(std::round(freqHz / hzPerSpectrumPair));

                // Clamp to make sure we don't go outside valid memory bounds.
                index = std::clamp(index, 0, spectrumLength-1);

                return ValidateIndex(spectrumLength, index);
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

            void setSampleRate(float newSampleRate)
            {
                if (newSampleRate != sampleRate)
                {
                    sampleRate = newSampleRate;
                    isCurveDirty = true;
                }
            }

            float setBandwidth(float newBandwidth = 1)
            {
                float b = std::clamp(newBandwidth, MinBandwidth, MaxBandwidth);
                if (b != bandwidth)
                {
                    bandwidth = b;
                    isCurveDirty = true;
                }
                return bandwidth;
            }

            float setCenterFrequencyOffset(float newOffset = 0)
            {
                float offset = std::clamp(newOffset, -OctaveHalfRange, +OctaveHalfRange);
                if (offset != frequencyOffsetOctaves)
                {
                    frequencyOffsetOctaves = offset;
                    isCurveDirty = true;
                }
                return frequencyOffsetOctaves;
            }

            FrequencyBand getFrequencyBand() const
            {
                return FrequencyBand{f1, fc, f2};
            }

            IndexRange getIndexRange() const
            {
                return IndexRange{indexLo, indexHi};
            }

            float getCurve(int index) const
            {
                if (index < indexLo || index > indexHi)
                    return 0;

                // Compensate for wider/narrower bands by emphasizing each frequency bin inversely.
                return curve.at(index) / bandwidth;
            }

            void updateCurve()
            {
                if (isCurveDirty)
                {
                    // Adjust the center frequency based on octave adjustment.
                    float factor = std::pow(2.0f, frequencyOffsetOctaves);

                    // Use the `bandwidth` parameter to adjust the low and high
                    // frequencies relative to the center frequency.

                    f1 = factor * (freqCenterHz - bandwidth*(freqCenterHz - freqLoHz));
                    fc = factor * freqCenterHz;
                    f2 = factor * (freqCenterHz + bandwidth*(freqHiHz - freqCenterHz));

                    indexLo = indexForFrequency(f1);
                    const int indexCenter = indexForFrequency(fc);
                    indexHi = indexForFrequency(f2);

                    // Calculate two cosine curves, but with different frequencies
                    // to match the different width frequency ranges [lo, center] and [center, hi].
                    halfCurve(indexCenter, indexLo);
                    halfCurve(indexCenter, indexHi);

                    isCurveDirty = false;
                }
            }
        };


        class DispersionBuffer
        {
        private:
            float dispersionAngle = -1;     // force setStandardDeviationAngle(0) to work when called by ctor
            std::vector<complex_t> factorList;

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
                    for (complex_t& factor : factorList)
                    {
                        float angle = radians * r.next();
                        factor = complex_t(std::cos(angle), std::sin(angle));
                    }
                }
            }

            complex_t getFactor(int spectrumIndex) const
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

                    // Lazy-update each band curve. This allows callers to mutate several parameters
                    // that affect the band curves, without recalculating the curves each time.
                    b.window.updateCurve();

                    // Apply and accumulate the band curve's effect on the signal.
                    IndexRange range = b.window.getIndexRange();
                    for (int spectrumIndex = range.lo; spectrumIndex <= range.hi; ++spectrumIndex)
                    {
                        int realIndex = 2*spectrumIndex;
                        int imagIndex = realIndex + 1;
                        float k = b.amplitude * b.window.getCurve(spectrumIndex);
                        complex_t z(k * inSpectrum[realIndex], k * inSpectrum[imagIndex]);
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

            void setBandWidth(int bandIndex, float bandwidth = 1)
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                {
                    Band& band = channelProcArray[c]->getBandMixer().band(bandIndex);
                    band.window.setBandwidth(bandwidth);
                }
            }

            void setCenterFrequencyOffset(int bandIndex, float octaves = 0)
            {
                for (int c = 0; c < MaxFrameChannels; ++c)
                {
                    Band& band = channelProcArray[c]->getBandMixer().band(bandIndex);
                    band.window.setCenterFrequencyOffset(octaves);
                }
            }
        };
    }
}
