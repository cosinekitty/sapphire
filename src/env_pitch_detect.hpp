#pragma once
#include <cmath>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    template <typename value_t, int maxChannels>
    class EnvPitchDetector
    {
    private:
        static_assert(maxChannels > 0);

        float centerFrequencyHz = 261.6255653005986;        // note C4 = 440 / (2**(3/4))

        int currentSampleRate = 0;

        value_t prevSignal[maxChannels];
        int ascendSamples[maxChannels];        // wavelength sample counters between consecutive ascending  zero-crossings
        int descendSamples[maxChannels];       // wavelength sample counters between consecutive descending zero-crossings
        value_t filteredWaveLength[maxChannels];

        using filter_t = StagedFilter<value_t, 3>;
        filter_t loCutFilter[maxChannels];
        filter_t hiCutFilter[maxChannels];
        filter_t jitterFilter[maxChannels];
        float loCutFrequency = 20;
        float hiCutFrequency = 3000;
        float jitterCornerFrequency = 10;
        int recoveryCountdown;                // how many samples remain before trying to filter again (CPU usage limiter)

        void updateWaveLength(int channel, int wavelengthSamples)
        {
            // The wavelengths we receive here will often be quite jittery.
            // We need to smooth them out with a lowpass filter of some kind.
            // The corner frequency of this filter will determine the responsiveness
            // (or SPEED) of the env/pitch detector.
            // TBD: should there be a single SPEED control for both? (Yes, probably.)
            // But if they were independent, would that be more interesting?
            // Or at least there could be an offset parameter.
            jitterFilter[channel].SetCutoffFrequency(jitterCornerFrequency);
            filteredWaveLength[channel] = jitterFilter[channel].UpdateLoPass(wavelengthSamples, currentSampleRate);
        }

    public:
        EnvPitchDetector()
        {
            initialize();
        }

        void initialize()
        {
            recoveryCountdown = 0;
            for (int c = 0; c < maxChannels; ++c)
            {
                prevSignal[c] = 0;
                ascendSamples[c] = 0;
                descendSamples[c] = 0;
                filteredWaveLength[c] = 0;
                // Reset all filters in case they went non-finite.
                loCutFilter[c].Reset();
                hiCutFilter[c].Reset();
                jitterFilter[c].Reset();
            }
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
                ++ascendSamples[c];
                ++descendSamples[c];

                // Feed through a bandpass filter that rejects DC and other frequencies below 20 Hz,
                // and also rejects very high frequencies.

                // Reject frequencies lower than we want to keep.
                loCutFilter[c].SetCutoffFrequency(loCutFrequency);
                float locut = loCutFilter[c].UpdateHiPass(inFrame[c], sampleRateHz);

                // Reject frequencies higher than we want to keep.
                // The band-pass result is our signal to feed through envelope and pitch detection.
                hiCutFilter[c].SetCutoffFrequency(hiCutFrequency);
                float signal = hiCutFilter[c].UpdateLoPass(locut, sampleRateHz);

                // Make sure we have a normal numeric value for our signal.
                if (!std::isfinite(signal))
                {
                    // AUTO-RESET when things go squirelly.
                    initialize();
                    signal = 0;

                    // Keep quiet for a quarter of a second. This prevents runaway CPU usage
                    // from initializing every single process() call!
                    recoveryCountdown = static_cast<int>(sampleRateHz/4);
                    return;
                }

                // Keep waiting until we go from negative to positive, or positive to negative,
                // with any number (zero or more) of 0-valued samples in between them.
                if (signal != 0)
                {
                    // Find (both ascending and descending), independently for each channel.
                    // Measure the interval between them, expressed in samples, called "wavelength".

                    if (signal * prevSignal[c] < 0)
                    {
                        if (signal > 0)
                        {
                            updateWaveLength(c, ascendSamples[c]);
                            ascendSamples[c] = 0;
                        }
                        else
                        {
                            updateWaveLength(c, descendSamples[c]);
                            descendSamples[c] = 0;
                        }
                    }

                    prevSignal[c] = signal;
                }

                // Convert wavelength [samples] to frequency [Hz] to pitch [V/OCT].
                // samplerate/wavelength: [samples/sec]/[samples] = [1/sec] = [Hz]
                if (filteredWaveLength[c] > sampleRateHz/4000)
                {
                    float frequencyHz = sampleRateHz / filteredWaveLength[c];
                    outPitchVoct[c] = log2(frequencyHz / centerFrequencyHz);
                }
            }
        }
    };
}
