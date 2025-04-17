#pragma once
#include <stdexcept>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_envelope_follower.hpp"
#include "sapphire_tapeloop.hpp"

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
            GateTriggerReceiver clockReceiver;
            int samplesSinceClockTrigger = 0;

            void initialize()
            {
                loop.initialize();
                clockReceiver.initialize();
                samplesSinceClockTrigger = 0;
            }
        };


        struct Message
        {
            int chainIndex = -1;
            Frame chainAudio;           // audio passed into the input layer of each tap through expander logic
            Frame originalAudio;        // audio frame from the input module (for MIX)
            Frame feedback;             // polyphonic modulation for the feedback parameter
            Frame clockVoltage;         // raw voltage read from the CLOCK port
            bool frozen = false;
            bool clear = false;
            bool isClockConnected = false;
            bool neonMode = false;
        };

        //--------------------------------------------------------------------
        // Module recognition helper functions.

        inline bool IsEcho(const Module* module)
        {
            return IsModelType(module, modelSapphireEcho);
        }

        inline bool IsEchoTap(const Module* module)
        {
            return IsModelType(module, modelSapphireEchoTap);
        }

        inline bool IsEchoOut(const Module* module)
        {
            return IsModelType(module, modelSapphireEchoOut);
        }

        inline bool IsEchoReceiver(const Module* module)
        {
            return IsEchoTap(module) || IsEchoOut(module);
        }

        inline bool IsEchoSender(const Module* module)
        {
            return IsEcho(module) || IsEchoTap(module);
        }

        //--------------------------------------------------------------------
        // Widget recognition helper functions.

        inline bool IsEcho(const ModuleWidget* widget)
        {
            return IsModelType(widget, modelSapphireEcho);
        }

        inline bool IsEchoTap(const ModuleWidget* widget)
        {
            return IsModelType(widget, modelSapphireEchoTap);
        }

        inline bool IsEchoOut(const ModuleWidget* widget)
        {
            return IsModelType(widget, modelSapphireEchoOut);
        }

        inline bool IsEchoReceiver(const ModuleWidget* widget)
        {
            return IsEchoTap(widget) || IsEchoOut(widget);
        }

        //--------------------------------------------------------------------

        struct PolyControls
        {
            ControlGroupIds delayTime;
            ControlGroupIds gain;
            ControlGroupIds pan;
            int sendLeftOutputId = -1;
            int sendRightOutputId = -1;
            int returnLeftInputId = -1;
            int returnRightInputId = -1;
            int clockInputId = -1;
        };
    }
}
