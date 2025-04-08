#pragma once
#include <stdexcept>
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

            int safeChannelCount() const
            {
                return VcvSafeChannelCount(nchannels);
            }

            float& at(int c)
            {
                const int nc = safeChannelCount();
                if (c < 0 || c >= nc)
                    throw std::out_of_range("invalid channel index in Frame");
                return sample[c];
            }

            const float& at(int c) const
            {
                const int nc = safeChannelCount();
                if (c < 0 || c >= nc)
                    throw std::out_of_range("invalid channel index in Frame (const)");
                return sample[c];
            }
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
            Frame feedback;             // polyphonic modulation for the feedback parameter
            bool frozen = false;
            bool clear = false;
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

        //--------------------------------------------------------------------

        struct PolyControls
        {
            ControlGroupIds delayTime;
        };
    }
}
