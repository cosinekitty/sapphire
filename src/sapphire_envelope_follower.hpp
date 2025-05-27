#pragma once
#include "sapphire_engine.hpp"

namespace Sapphire
{
    class EnvelopeFollower
    {
    private:
        float prevSampleRate{};
        float envAttack{};
        float envDecay{};
        float envelope{};
        LoHiPassFilter<float> filter;

    public:
        void initialize()
        {
            envelope = 0;
            filter.Reset();
            filter.SetCutoffFrequency(80);
        }

        float update(float signal, int sampleRate)
        {
            // Based on Surge XT Tree Monster's envelope follower:
            // https://github.com/surge-synthesizer/sst-effects/blob/main/include/sst/effects-shared/TreemonsterCore.h
            if (sampleRate != prevSampleRate)
            {
                prevSampleRate = sampleRate;
                envAttack = std::pow(0.01, 1.0 / (0.003*sampleRate));
                envDecay  = std::pow(0.01, 1.0 / (0.150*sampleRate));
            }
            float v = std::abs(signal);
            float k = (v > envelope) ? envAttack : envDecay;
            envelope = k*(envelope - v) + v;

            const float correction = (5.0 / 4.783);     // experimentally derived using sinewave input
            filter.Update(correction * envelope, sampleRate);
            return filter.LoPass();
        }
    };
}
