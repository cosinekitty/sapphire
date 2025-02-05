#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    const int NO_PITCH_VOLTS = -10;    // V/OCT = -10V indicates no pitch detected at all

    template <typename value_t, int filterLayers>
    struct EnvPitchChannelInfo
    {
        value_t prevSignal;
        int ascendSamples;        // wavelength sample counters between consecutive ascending  zero-crossings
        int descendSamples;       // wavelength sample counters between consecutive descending zero-crossings
        int rawWaveLengthAscend;
        int rawWaveLengthDescend;
        value_t filteredWaveLength;
        bool first_thresh;

        using filter_t = StagedFilter<value_t, filterLayers>;
        filter_t loCutFilter;
        filter_t hiCutFilter;
        filter_t jitterFilter;

        value_t envAttack = 0;
        value_t envDecay = 0;
        value_t envelope = 0;
        int prevSampleRate = 0;

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
            first_thresh = true;

            // Reset all filters in case they went non-finite.
            loCutFilter.Reset();
            hiCutFilter.Reset();
            jitterFilter.Reset();

            envelope = 0;
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
            if (filteredWaveLength >= 10)
            {
                value_t frequencyHz = sampleRateHz / filteredWaveLength;
                return std::log2(frequencyHz / centerFrequencyHz);
            }
            return NO_PITCH_VOLTS;
        }

        value_t updateAmplitude(value_t signal, int sampleRate)
        {
            // Based on Surge XT Tree Monster's envelope follower:
            // https://github.com/surge-synthesizer/sst-effects/blob/main/include/sst/effects-shared/TreemonsterCore.h
            if (sampleRate != prevSampleRate)
            {
                prevSampleRate = sampleRate;
                envAttack = std::pow(0.01, 1.0 / (0.005*sampleRate));
                envDecay  = std::pow(0.01, 1.0 / (0.500*sampleRate));
            }
            value_t v = std::abs(signal);
            value_t k = (v > envelope) ? envAttack : envDecay;
            envelope = k*(envelope - v) + v;
            return envelope;
        }
    };


    template <typename value_t, int maxChannels, int filterLayers = 1>
    class EnvPitchDetector
    {
    private:
        static_assert(maxChannels > 0);

        int currentSampleRate = 0;
        value_t centerFrequencyHz = 261.6255653005986;        // note C4 = 440 / (2**(3/4))
        value_t loCutFrequency = 20;
        value_t hiCutFrequency = 3000;
        value_t jitterCornerFrequency = 5;
        int recoveryCountdown = 0;         // how many samples remain before trying to filter again (CPU usage limiter)
        const int smallestWavelength = 16;
        value_t thresh = 0;     // amplitude to reach before considering pitch to be significant

        using info_t = EnvPitchChannelInfo<value_t, filterLayers>;
        std::vector<info_t> info;

        void setThreshold(value_t knob = -24)
        {
            value_t db = std::clamp(knob, static_cast<value_t>(-96), static_cast<value_t>(0));
            thresh = std::pow(static_cast<value_t>(10), static_cast<value_t>(db/20));
        }

        void updateWaveLength(info_t& q, int& wavelengthSamples, int samplesSinceCrossing)
        {
            // The wavelengths we receive here will often be quite jittery.
            // We need to smooth them out with a lowpass filter of some kind.
            // The corner frequency of this filter will determine the responsiveness
            // (or SPEED) of the env/pitch detector.
            // TBD: should there be a single SPEED control for both? (Yes, probably.)
            // But if they were independent, would that be more interesting?
            // Or at least there could be an offset parameter.

            // Don't pollute the filter with ridiculous values!
            // It's better to bail out and ignore this wavelength if it
            // is invalid or too short to take seriously.
            if (wavelengthSamples < smallestWavelength)
                return;

            const value_t rawFrequencyHz = currentSampleRate / static_cast<value_t>(wavelengthSamples);
            if (rawFrequencyHz < loCutFrequency || rawFrequencyHz > hiCutFrequency)
                return;

            if (10*samplesSinceCrossing > currentSampleRate)    // 0.1 seconds since we saw a zero crossing?
            {
                // It has been too long since we saw a valid wavelength.
                // Start ignoring this wavelength as a pitch signal.
                wavelengthSamples = 0;
                return;
            }

            q.jitterFilter.SetCutoffFrequency(jitterCornerFrequency);

            if (q.first_thresh)
            {
                q.first_thresh = false;
                q.filteredWaveLength = q.jitterFilter.SnapLoPass(wavelengthSamples);
            }
            else
            {
                q.filteredWaveLength = q.jitterFilter.UpdateLoPass(wavelengthSamples, currentSampleRate);
            }
        }

        void processChannel(int c, value_t input, value_t& outEnvelope, value_t& outPitchVoct)
        {
            info_t& q = info.at(c);

            ++q.ascendSamples;
            ++q.descendSamples;

            outEnvelope = q.updateAmplitude(input, currentSampleRate);

            // Feed through a bandpass filter that rejects DC and other frequencies below 20 Hz,
            // and also rejects very high frequencies.
            value_t signal = q.bandpass(input, loCutFrequency, hiCutFrequency, currentSampleRate);

            // Make sure we have a normal numeric value for our signal.
            if (!std::isfinite(signal))
            {
                // AUTO-RESET when things go squirelly.
                initialize();

                // Keep quiet for a quarter of a second. This prevents runaway CPU usage
                // from initializing every single process() call!
                recoveryCountdown = static_cast<int>(currentSampleRate/4);
                return;
            }

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
                        if (q.ascendSamples >= smallestWavelength && signal > thresh)
                            q.rawWaveLengthAscend = q.ascendSamples;
                        q.ascendSamples = 0;
                    }
                    else
                    {
                        if (q.descendSamples >= smallestWavelength && signal < -thresh)
                            q.rawWaveLengthDescend = q.descendSamples;
                        q.descendSamples = 0;
                    }
                }

                q.prevSignal = signal;
            }

            updateWaveLength(q, q.rawWaveLengthAscend,  q.ascendSamples);
            updateWaveLength(q, q.rawWaveLengthDescend, q.descendSamples);
            outPitchVoct = q.pitch(currentSampleRate, centerFrequencyHz);
        }

    public:
        EnvPitchDetector()
        {
            info.resize(maxChannels);
            setThreshold();
            initialize();
        }

        void initialize()
        {
            recoveryCountdown = 0;
            for (info_t& q : info)
                q.initialize();
        }

        int process(
            int numChannels,
            int sampleRateHz,
            const value_t* inFrame,     // input  array [numChannels]
            value_t* outEnvelope,       // output array [numChannels]
            value_t* outPitchVoct)      // output array [numChannels]
        {
            const int nc = std::clamp(numChannels, 0, maxChannels);

            // Initialize output to whatever we deem a quiet state.
            for (int c = 0; c < nc; ++c)
            {
                outEnvelope[c] = 0;
                outPitchVoct[c] = NO_PITCH_VOLTS;
            }

            if (nc > 0)    // avoid division by zero later
            {
                // See if we are in a CPU-protective quiet period.
                if (recoveryCountdown > 0)
                {
                    --recoveryCountdown;
                }
                else
                {
                    if (sampleRateHz != currentSampleRate)
                    {
                        // Reset: it's OK to glitch a little when sample rate changes,
                        // but we don't want to produce erroneous pitch/env information.
                        initialize();
                        currentSampleRate = sampleRateHz;
                    }

                    for (int c = 0; c < nc; ++c)
                        processChannel(c, inFrame[c], outEnvelope[c], outPitchVoct[c]);
                }
            }

            return nc;      // number of channels written to outEnvelope[], outPitchVoct[].
        }
    };
}
