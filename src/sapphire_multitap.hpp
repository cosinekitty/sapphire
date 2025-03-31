#pragma once
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
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
            s.nchannels = std::max(a.nchannels, b.nchannels);

            for (int c = 0; c < a.nchannels; ++c)
                s.sample[c] += a.sample[c];

            for (int c = 0; c < b.nchannels; ++c)
                s.sample[c] += b.sample[c];

            return s;
        }

        struct Message
        {
            int chainIndex = -1;
            Frame audio;
        };

        struct InputState
        {
            // Holds all the input port values and parameter values we need
            // to update the tape recorder model, while remaining module-agnostic.
            // This way different modules with different port IDs and parameter IDs
            // can share the complicated part of the code.
            Frame audio;
        };

        struct OutputState
        {
            // Receives module-agnostic values for output ports.
        };

        struct Result
        {
            Message message;
            OutputState output;
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
