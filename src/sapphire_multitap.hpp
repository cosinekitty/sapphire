#pragma once
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "multitap_inloop_panel.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        class EnvelopeFollower
        {
        private:
            float prevSampleRate{};
            float envAttack{};
            float envDecay{};
            float envelope{};

        public:
            void initialize()
            {
                envelope = 0;
            }

            float update(float signal, int sampleRate)
            {
                // Based on Surge XT Tree Monster's envelope follower:
                // https://github.com/surge-synthesizer/sst-effects/blob/main/include/sst/effects-shared/TreemonsterCore.h
                if (sampleRate != prevSampleRate)
                {
                    prevSampleRate = sampleRate;
                    envAttack = std::pow(0.01, 1.0 / (0.005*sampleRate));
                    envDecay  = std::pow(0.01, 1.0 / (0.500*sampleRate));
                }
                float v = std::abs(signal);
                float k = (v > envelope) ? envAttack : envDecay;
                envelope = k*(envelope - v) + v;
                return envelope;
            }
        };

        inline int SafeChannelCount(int count)
        {
            return std::clamp(count, 0, PORT_MAX_CHANNELS);
        }

        struct Frame
        {
            int nchannels = 0;
            float sample[PORT_MAX_CHANNELS]{};
        };

        inline Frame operator+ (const Frame& a, const Frame& b)
        {
            Frame s;
            s.nchannels = SafeChannelCount(std::max(a.nchannels, b.nchannels));

            for (int c = 0; c < a.nchannels; ++c)
                s.sample[c] += a.sample[c];

            for (int c = 0; c < b.nchannels; ++c)
                s.sample[c] += b.sample[c];

            return s;
        }

        struct Message
        {
            int chainIndex = -1;
            Frame chainAudio;           // audio passed into the input layer of each tap through expander logic
            Frame originalAudio;        // audio frame from the input module (for MIX)
            bool frozen = false;
        };

        //--------------------------------------------------------------------
        // Module recognition helper functions.

        inline bool IsInLoop(const Module* module)
        {
            return IsModelType(module, modelSapphireInLoop);
        }

        inline bool IsLoop(const Module* module)
        {
            return IsModelType(module, modelSapphireLoop);
        }

        inline bool IsOutLoop(const Module* module)
        {
            return IsModelType(module, modelSapphireOutLoop);
        }
    }
}
