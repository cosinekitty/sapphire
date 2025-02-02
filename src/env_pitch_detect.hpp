#pragma once
#include <cmath>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    template <typename value_t, int filterLayers>
    struct EnvPitchChannelInfo
    {
        value_t prevSignal;
        int ascendSamples;        // wavelength sample counters between consecutive ascending  zero-crossings
        int descendSamples;       // wavelength sample counters between consecutive descending zero-crossings
        int rawWaveLengthAscend;
        int rawWaveLengthDescend;
        value_t filteredWaveLength;

        using filter_t = StagedFilter<value_t, filterLayers>;
        filter_t loCutFilter;
        filter_t hiCutFilter;
        filter_t jitterFilter;
        filter_t amplFilter;

        EnvPitchChannelInfo()
        {
            initialize();
        }

        void initialize()
        {
            prevSignal = 0;
            ascendSamples = 0;
            descendSamples = 0;
            rawWaveLengthAscend = 0;
            rawWaveLengthDescend = 0;
            filteredWaveLength = 0;

            // Reset all filters in case they went non-finite.
            loCutFilter.Reset();
            hiCutFilter.Reset();
            jitterFilter.Reset();
            amplFilter.Reset();
        }

        value_t bandpass(value_t input, value_t loFreq, value_t hiFreq, int sampleRateHz)
        {
            loCutFilter.SetCutoffFrequency(loFreq);
            value_t locut = loCutFilter.UpdateHiPass(input, sampleRateHz);

            hiCutFilter.SetCutoffFrequency(hiFreq);
            return hiCutFilter.UpdateLoPass(locut, sampleRateHz);
        }

        value_t pitch(int sampleRateHz, value_t centerFrequencyHz) const
        {
            // Convert wavelength [samples] to frequency [Hz] to pitch [V/OCT].
            // samplerate/wavelength: [samples/sec]/[samples] = [1/sec] = [Hz]
            if (filteredWaveLength > sampleRateHz/4000)
            {
                value_t frequencyHz = sampleRateHz / filteredWaveLength;
                return std::log2(frequencyHz / centerFrequencyHz);
            }
            return -10;     // sentinel value: -10 on a V/OCT scale represents the frequency 0.255 Hz.
        }
    };


    template <typename value_t, int maxChannels, int filterLayers = 3>
    class EnvPitchDetector
    {
    private:
        static_assert(maxChannels > 0);

        int currentSampleRate = 0;
        value_t centerFrequencyHz = 261.6255653005986;        // note C4 = 440 / (2**(3/4))
        value_t loCutFrequency = 20;
        value_t hiCutFrequency = 3000;
        value_t jitterCornerFrequency = 10;
        value_t amplCornerFrequency = 10;
        int recoveryCountdown = 0;         // how many samples remain before trying to filter again (CPU usage limiter)

        using info_t = EnvPitchChannelInfo<value_t, filterLayers>;
        std::vector<info_t> info;

        value_t updateAmplitude(int channel, value_t signal)
        {
            // Square the signal and filter the result.
            // This gives us a time-smeared measure of power.
            info_t& q = info.at(channel);
            q.amplFilter.SetCutoffFrequency(amplCornerFrequency);
            return q.amplFilter.UpdateLoPass(signal*signal, currentSampleRate);
        }

        void updateWaveLength(int channel, int wavelengthSamples)
        {
            // The wavelengths we receive here will often be quite jittery.
            // We need to smooth them out with a lowpass filter of some kind.
            // The corner frequency of this filter will determine the responsiveness
            // (or SPEED) of the env/pitch detector.
            // TBD: should there be a single SPEED control for both? (Yes, probably.)
            // But if they were independent, would that be more interesting?
            // Or at least there could be an offset parameter.

            // Don't pollute the filter with ridiculous values!
            // It's better to bail out and ignore this wavelength if it's crazy.
            if (wavelengthSamples <= 0)
                return;

            const float rawFrequencyHz = currentSampleRate / static_cast<float>(wavelengthSamples);
            if (rawFrequencyHz < loCutFrequency || rawFrequencyHz > hiCutFrequency)
                return;

            info_t& q = info.at(channel);
            q.jitterFilter.SetCutoffFrequency(jitterCornerFrequency);
            q.filteredWaveLength = q.jitterFilter.UpdateLoPass(wavelengthSamples, currentSampleRate);
        }

    public:
        EnvPitchDetector()
        {
            info.resize(maxChannels);
            initialize();
        }

        void initialize()
        {
            recoveryCountdown = 0;
            for (info_t& q : info)
                q.initialize();
        }

        void process(
            int numChannels,
            int sampleRateHz,
            const value_t* inFrame,     // input  array [numChannels]
            value_t* outEnvelope,       // output array [numChannels]
            value_t* outPitchVoct)      // output array [numChannels]
        {
            assert(numChannels <= maxChannels);

            // Initialize output to whatever we deem a quiet state.
            for (int c = 0; c < numChannels; ++c)
            {
                outEnvelope[c] = 0;
                outPitchVoct[c] = 0;
            }

            if (numChannels < 1)    // avoid division by zero later
                return;

            // See if we are in a CPU-protective quiet period.
            if (recoveryCountdown > 0)
            {
                --recoveryCountdown;
                return;
            }

            if (sampleRateHz != currentSampleRate)
            {
                // Reset: it's OK to glitch a little when sample rate changes,
                // but we don't want to produce erroneous pitch/env information.
                initialize();
                currentSampleRate = sampleRateHz;
            }

            for (int c = 0; c < numChannels; ++c)
            {
                info_t& q = info.at(c);

                ++q.ascendSamples;
                ++q.descendSamples;

                // Feed through a bandpass filter that rejects DC and other frequencies below 20 Hz,
                // and also rejects very high frequencies.
                value_t signal = q.bandpass(inFrame[c], loCutFrequency, hiCutFrequency, sampleRateHz);

                // Make sure we have a normal numeric value for our signal.
                if (!std::isfinite(signal))
                {
                    // AUTO-RESET when things go squirelly.
                    initialize();

                    // Keep quiet for a quarter of a second. This prevents runaway CPU usage
                    // from initializing every single process() call!
                    recoveryCountdown = static_cast<int>(sampleRateHz/4);
                    return;
                }

                outEnvelope[c] = updateAmplitude(c, signal);

                // Keep waiting until we go from negative to positive, or positive to negative,
                // with any number (zero or more) of 0-valued samples in between them.
                if (signal != 0)
                {
                    // Find (both ascending and descending), independently for each channel.
                    // Measure the interval between them, expressed in samples, called "wavelength".

                    if (signal * q.prevSignal < 0)
                    {
                        if (signal > 0)
                        {
                            q.rawWaveLengthAscend = q.ascendSamples;
                            q.ascendSamples = 0;
                        }
                        else
                        {
                            q.rawWaveLengthDescend = q.descendSamples;
                            q.descendSamples = 0;
                        }
                    }

                    q.prevSignal = signal;
                }

                updateWaveLength(c, q.rawWaveLengthAscend);
                updateWaveLength(c, q.rawWaveLengthDescend);
                outPitchVoct[c] = q.pitch(sampleRateHz, centerFrequencyHz);
            }
        }
    };
}
