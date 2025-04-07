#pragma once
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_envelope_follower.hpp"
#include "sapphire_tapeloop.hpp"
#include "multitap_inloop_panel.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct Frame
        {
            int nchannels = 0;
            float sample[PORT_MAX_CHANNELS]{};
        };

        inline Frame operator+ (const Frame& a, const Frame& b)
        {
            Frame s;
            s.nchannels = VcvSafeChannelCount(std::max(a.nchannels, b.nchannels));

            for (int c = 0; c < a.nchannels; ++c)
                s.sample[c] += a.sample[c];

            for (int c = 0; c < b.nchannels; ++c)
                s.sample[c] += b.sample[c];

            return s;
        }


        struct ChannelInfo
        {
            TapeLoop loop;

            void initialize()
            {
                loop.initialize();
            }
        };


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
