#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "sapphire_engine.hpp"
#include "sauce_engine.hpp"

namespace Sapphire
{
    const int NO_PITCH_VOLTS = -10;    // V/OCT = -10V indicates no pitch detected at all

    namespace Env
    {
        const int MinThreshold = -96;       // decibels
        const int MaxThreshold = 0;         // decibels
        const int DefaultThreshold = -30;   // decibels

        const int SmallestWaveLength = 16;  // samples
    }

    template <typename value_t>
    struct EnvPitchChannelInfo
    {
        value_t prevSignal;
        int ascendSamples;        // wavelength sample counters between consecutive ascending  zero-crossings
        int descendSamples;       // wavelength sample counters between consecutive descending zero-crossings
        int samplesSincePitchDetected;
        int rawWaveLengthAscend;
        int rawWaveLengthDescend;
        value_t filteredWaveLength;
        bool first_thresh;

        using filter_t = Gravy::SingleChannelGravyEngine<value_t>;
        filter_t pitchFilter;

        value_t envAttack = 0;
        value_t envDecay = 0;
        value_t envelope = 0;
        int prevSampleRate = 0;

        value_t speed;
        value_t threshold;

        EnvPitchChannelInfo()
        {
            initialize();
        }

        void initialize()
        {
            prevSignal = 0;
            ascendSamples = 0;
            descendSamples = 0;
            samplesSincePitchDetected = 0;
            rawWaveLengthAscend = 0;
            rawWaveLengthDescend = 0;
            filteredWaveLength = 0;
            first_thresh = true;
            pitchFilter.initialize();
            envelope = 0;
            setSpeed(0.5);
            setThreshold(Env::DefaultThreshold);
        }

        value_t pitch(int sampleRateHz, value_t centerFrequencyHz) const
        {
            // Convert wavelength [samples] to frequency [Hz] to pitch [V/OCT].
            // samplerate/wavelength: [samples/sec]/[samples] = [1/sec] = [Hz]
            if (filteredWaveLength >= Env::SmallestWaveLength)
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
            value_t v;
            if (10 * samplesSincePitchDetected > sampleRate)
                v = 0;
            else
                v = std::abs(signal);
            value_t k = (v > envelope) ? envAttack : envDecay;
            envelope = k*(envelope - v) + v;
            return envelope;
        }

        void setSpeed(value_t knob)
        {
            value_t qs = std::clamp(knob, static_cast<value_t>(0), static_cast<value_t>(1));
            qs *= qs;   // square
            qs *= qs;   // fourth power
            speed = 0.9999 - qs*(0.0999 / 128);
        }

        void setThreshold(value_t knob)
        {
            value_t db = std::clamp(knob, static_cast<value_t>(Env::MinThreshold), static_cast<value_t>(Env::MaxThreshold));
            threshold = std::pow(static_cast<value_t>(10), static_cast<value_t>(db/20));
        }
    };


    template <typename value_t, int maxChannels>
    class EnvPitchDetector
    {
    private:
        static_assert(maxChannels > 0);

        int currentSampleRate = 0;
        value_t centerFrequencyHz = 261.6255653005986;        // note C4 = 440 / (2**(3/4))
        int recoveryCountdown = 0;         // how many samples remain before trying to filter again (CPU usage limiter)
        static constexpr value_t envelopeCorrection = 1.0324964430935937;   // experimentally derived envelope correction factor for sinewave input

        using info_t = EnvPitchChannelInfo<value_t>;
        std::vector<info_t> info;

        void updateWaveLength(info_t& q, int& wavelengthSamples, int samplesSinceCrossing)
        {
            // Don't pollute the filter with ridiculous values!
            // It's better to bail out and ignore this wavelength if it
            // is invalid or too short to take seriously.
            if (wavelengthSamples < Env::SmallestWaveLength)
                return;

            if (10*samplesSinceCrossing > currentSampleRate)    // 0.1 seconds since we saw a zero crossing?
            {
                // It has been too long since we saw a valid wavelength.
                // Start ignoring this wavelength as a pitch signal.
                wavelengthSamples = 0;
                q.first_thresh = true;
                return;
            }

            if (q.first_thresh)
            {
                q.first_thresh = false;
                q.filteredWaveLength = wavelengthSamples;
            }
            else
            {
                value_t numberOfSteps = static_cast<value_t>(48000) / currentSampleRate;
                for (int i = 0; i < numberOfSteps; ++i)
                    q.filteredWaveLength = q.speed*q.filteredWaveLength + (1-q.speed)*wavelengthSamples;
            }
        }

        void processChannel(int c, value_t input, value_t& outEnvelope, value_t& outPitchVoct)
        {
            info_t& q = info.at(c);

            ++q.ascendSamples;
            ++q.descendSamples;
            if (q.samplesSincePitchDetected < 1000000)
                ++q.samplesSincePitchDetected;

            FilterResult<float> result = q.pitchFilter.process(currentSampleRate, input);
            value_t signal = result.bandpass;

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
                        if (q.ascendSamples >= Env::SmallestWaveLength && signal > q.threshold)
                        {
                            q.rawWaveLengthAscend = q.ascendSamples;
                            q.samplesSincePitchDetected = 0;
                        }
                        q.ascendSamples = 0;
                    }
                    else
                    {
                        if (q.descendSamples >= Env::SmallestWaveLength && signal < -q.threshold)
                        {
                            q.rawWaveLengthDescend = q.descendSamples;
                            q.samplesSincePitchDetected = 0;
                        }
                        q.descendSamples = 0;
                    }
                }

                q.prevSignal = signal;
            }

            updateWaveLength(q, q.rawWaveLengthAscend,  q.ascendSamples);
            updateWaveLength(q, q.rawWaveLengthDescend, q.descendSamples);
            outEnvelope = envelopeCorrection * q.updateAmplitude(input, currentSampleRate);
            outPitchVoct = q.pitch(currentSampleRate, centerFrequencyHz);
        }

    public:
        EnvPitchDetector()
        {
            info.resize(maxChannels);
            for (int c = 0; c < maxChannels; ++c)
            {
                setThreshold(Env::DefaultThreshold, c);
                setSpeed(0.5, c);
                setFrequency(0, c);
                setResonance(0.25, c);
            }
            initialize();
        }

        void initialize()
        {
            recoveryCountdown = 0;
            for (info_t& q : info)
                q.initialize();
        }

        void setThreshold(value_t knob, int channel)
        {
            info_t& q = info.at(channel);
            q.setThreshold(knob);
        }

        void setSpeed(value_t knob, int channel)
        {
            info_t& q = info.at(channel);
            q.setSpeed(knob);
        }

        void setFrequency(value_t knob, int channel)
        {
            info_t& q = info.at(channel);
            q.pitchFilter.setFrequency(knob);
        }

        void setResonance(value_t knob, int channel)
        {
            info_t& q = info.at(channel);
            q.pitchFilter.setResonance(knob);
        }

        int process(
            int numChannels,
            int sampleRateHz,
            const value_t* inFrame,     // input  array [numChannels]
            value_t* outEnvelope,       // output array [numChannels]
            value_t* outPitchVoct)      // output array [numChannels]
        {
            const int nc = std::clamp(numChannels, 0, maxChannels);
            if (nc > 0)    // avoid division by zero later
            {
                // Initialize output to whatever we deem a quiet state.
                for (int c = 0; c < nc; ++c)
                {
                    outEnvelope[c] = 0;
                    outPitchVoct[c] = NO_PITCH_VOLTS;
                }

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
