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

            Frame& operator += (const Frame& other)
            {
                nchannels = VcvSafeChannelCount(std::max(nchannels, other.nchannels));

                const int nc = other.safeChannelCount();
                for (int c = 0; c < nc; ++c)
                    sample[c] += other.sample[c];

                return *this;
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

        enum class TapInputRouting
        {
            Serial,
            Parallel,
            Loop,
            LEN
        };

        inline char InputRoutingChar(TapInputRouting routing)
        {
            switch (routing)
            {
            case TapInputRouting::Serial:
                return 'S';
            case TapInputRouting::Parallel:
                return 'P';
            case TapInputRouting::Loop:
                return 'L';
            default:
                return '?';
            }
        }

        enum class ReverseOutput
        {
            Mix,            // if reversed, the backwards audio goes to the output mix only
            MixAndChain,    // if reversed, the backwards audio also goes to the next tap in the chain
        };

        struct Message      // data that flows through the expander chain left-to-right.
        {
            int chainIndex = -1;
            Frame chainAudio;           // audio passed into the input layer of each tap through expander logic
            Frame originalAudio;        // audio frame from the input module (for final output mix)
            Frame summedAudio;          // the sum of outputs from all taps to the left (for final output mix)
            Frame feedback;             // polyphonic modulation for the feedback parameter
            Frame clockVoltage;         // raw voltage read from the CLOCK port
            bool frozen = false;
            bool clear = false;
            bool isClockConnected = false;
            bool neonMode = false;
            TapInputRouting inputRouting = TapInputRouting::Serial;
            InterpolatorKind interpolatorKind = InterpolatorKind::Linear;
            bool polyphonic = false;    // resolves stereo/polyphonic ambiguity when nchannels==2
        };

        struct BackwardMessage      // data that flows through the expander chain right-to-left.
        {
            Frame loopAudio;        // the chain output from the rightmost EchoTap (used for Loop mode)
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

        //--------------------------------------------------------------------

        struct InitChainNode
        {
            json_t* json;
            int64_t moduleId;

            explicit InitChainNode(Module* module)
                : json(module->toJson())
                , moduleId(module->id)
                {}
        };


        struct InitChainAction : history::Action
        {
            std::vector<InitChainNode> list;

            explicit InitChainAction(const std::vector<InitChainNode>& _list)
                : list(_list)
            {
                name = "initialize Echo expander chain";
            }

            virtual ~InitChainAction()
            {
                for (const InitChainNode& node : list)
                    json_decref(node.json);
            }

            void undo() override;
            void redo() override;
        };
    }
}
