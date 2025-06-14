#pragma once
#include <stdexcept>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_envelope_follower.hpp"
#include "sapphire_tapeloop.hpp"
#include "sapphire_smoother.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct Fraction
        {
            int numer;
            int denom;
            std::string name;

            Fraction(int _numer, int _denom, const std::string& _name)
                : numer(_numer)
                , denom(_denom)
                , name(_name)
            {
                if (numer <= 0) throw std::out_of_range("Fraction numerator was not a positive integer.");
                if (denom <= 0) throw std::out_of_range("Fraction denominator was not a positive integer.");
            }

            float value() const
            {
                return static_cast<float>(numer) / static_cast<float>(denom);
            }

            static std::string formatFraction(int numer, int denom)
            {
                std::string numerText = std::to_string(numer);
                if (denom == 1)
                    return numerText;
                return numerText + "/" + std::to_string(denom);
            }

            std::string format() const
            {
                return
                    "CLOCK x " + formatFraction(numer, denom) + "\n" +
                    "RATE x "  + formatFraction(denom, numer) + "\n" +
                    "(" + name + ")"
                ;
            }
        };

        const Fraction& PickClosestFraction(float ratio);

        enum class PortLabelMode
        {
            Stereo = -2,
            Mono = -1,
            Poly = 0,       // add the number of channels: +1..16
        };


        enum class TimeMode
        {
            Seconds,
            ClockSync,
            LEN,

            Default = Seconds,
        };


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

            Frame& operator *= (float scalar)
            {
                const int nc = safeChannelCount();
                for (int c = 0; c < nc; ++c)
                    sample[c] *= scalar;
                return *this;
            }

            void clear()
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    sample[c] = 0;
            }

            void initialize()
            {
                nchannels = 0;
                clear();
            }

            void rootMeanSquare(unsigned nsamples)
            {
                if (nsamples > 0)
                {
                    const int nc = safeChannelCount();
                    for (int c = 0; c < nc; ++c)
                        sample[c] = std::sqrt(sample[c] / nsamples);
                }
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


        inline Frame operator*(float scalar, const Frame& frame)
        {
            Frame product;
            product.nchannels = frame.safeChannelCount();
            for (int c = 0; c < product.nchannels; ++c)
                product.sample[c] = scalar * frame.sample[c];
            return product;
        }


        struct ChannelInfo
        {
            TapeLoop loop;
            GateTriggerReceiver clockReceiver;
            int samplesSinceClockTrigger = 0;
            bool isReceivingTriggers = false;
            float clockSyncTime = 0;
            EnvelopeFollower env;

            void initialize()
            {
                loop.initialize();
                clockReceiver.initialize();
                samplesSinceClockTrigger = 0;
                isReceivingTriggers = false;
                env.initialize();
                clockSyncTime = 0;
            }
        };

        enum class TapInputRouting
        {
            Parallel,
            Serial,
            LEN,

            Default = Serial,
        };

        inline char InputRoutingChar(TapInputRouting routing)
        {
            switch (routing)
            {
            case TapInputRouting::Parallel:
                return 'P';
            case TapInputRouting::Serial:
                return 'S';
            default:
                return '?';
            }
        }

        enum class ClockSignalFormat
        {
            Pulses,     // interval between rising edges defines the delay time
            Voct,       // V/OCT signal where 0 volts = 1 Hz.
            LEN,

            Default = Pulses
        };

        struct Message      // data that flows through the expander chain left-to-right.
        {
            bool valid = false;
            int chainIndex = -1;
            Frame chainAudio;           // audio passed into the input layer of each tap through expander logic
            Frame originalAudio;        // audio frame from the input module (for final output mix)
            Frame summedAudio;          // the sum of outputs from all taps to the left (for final output mix)
            int soloCount = 0;          // how many taps have solo enabled
            Frame soloAudio;            // the sum of all output audio for taps with solo enabled
            Frame feedback;             // polyphonic modulation for the feedback parameter
            Frame clockVoltage;         // raw voltage read from the CLOCK port
            float freezeMix = 0;        // 0=thawed, 1=frozen, in between for linear crossfade
            bool clear = false;
            bool isClockConnected = false;
            ClockSignalFormat clockSignalFormat = ClockSignalFormat::Default;
            bool neonMode = false;
            TapInputRouting inputRouting = TapInputRouting::Default;
            float routingSmooth = 1;    // ducking factor just before/after changing inputRouting
            InterpolatorKind interpolatorKind = InterpolatorKind::Linear;
            bool polyphonic = false;    // selects desired output format: false=stereo(L,R), true=polyphonic(L)
            bool musicalInterval = false;
        };

        struct BackwardMessage      // data that flows through the expander chain right-to-left.
        {
            bool valid = false;
            Frame loopAudio;        // the chain output from the rightmost EchoTap (used for Serial mode)
            int soloCount = 0;      // how many taps are soloing right now
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
            int revFlipInputId = -1;
            int revFlipButtonId = -1;
            int sendReturnButtonId = -1;
            int muteButtonId = -1;
            int soloButtonId = -1;
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

        //--------------------------------------------------------------------


        enum class ReverseFlipMode
        {
            Forward,
            Reverse,
            Flip
        };


        class ReverseComboSmoother : public EnumSmoother<ReverseFlipMode>
        {
        public:
            explicit ReverseComboSmoother()
                : EnumSmoother(ReverseFlipMode::Forward, "" /*not saved/loded via json*/)
                {}

            void select(float forwardSample, float reverseSample, float& mixerSample, float& feedSample) const
            {
                switch (currentValue)
                {
                case ReverseFlipMode::Forward:
                default:
                    mixerSample = forwardSample;
                    feedSample = forwardSample;
                    break;

                case ReverseFlipMode::Reverse:
                    mixerSample = reverseSample;
                    feedSample = forwardSample;
                    break;

                case ReverseFlipMode::Flip:
                    mixerSample = forwardSample;
                    feedSample = reverseSample;
                    break;
                }

                mixerSample *= getGain();
                feedSample  *= getGain();
            }

            bool isReverseNeeded() const
            {
                return currentValue != ReverseFlipMode::Forward;
            }
        };


        enum class SendReturnLocation
        {
            BeforeDelay,
            AfterDelay,
            LEN
        };


        class SendReturnLocationSmoother : public EnumSmoother<SendReturnLocation>
        {
        public:
            explicit SendReturnLocationSmoother()
                : EnumSmoother(SendReturnLocation::BeforeDelay, "sendReturnLocation")
                {}
        };


        struct EnvelopeOutputPort : SapphirePort
        {
            void appendContextMenu(Menu* menu) override;
        };


        struct ClockSyncState
        {
            int64_t moduleId{};
            TimeMode oldTimeMode{};

            explicit ClockSyncState(int64_t _moduleId, TimeMode _oldTimeMode)
                : moduleId(_moduleId)
                , oldTimeMode(_oldTimeMode)
                {}
        };


        struct ToggleAllClockSyncAction : history::Action
        {
            std::vector<ClockSyncState> stateList;
            TimeMode newTimeMode{};

            explicit ToggleAllClockSyncAction(TimeMode _newTimeMode)
                : newTimeMode(_newTimeMode)
            {
                name = "toggle all clock sync";
            }

            void undo() override;
            void redo() override;
        };


        struct PolyEnvelopeState
        {
            const int64_t moduleId;
            const bool state;

            explicit PolyEnvelopeState(int64_t _moduleId, bool _state)
                : moduleId(_moduleId)
                , state(_state)
                {}
        };


        struct ToggleAllPolyphonicEnvelopeAction : history::Action
        {
            const int64_t moduleId;
            const bool newState;
            std::vector<PolyEnvelopeState> stateList;

            explicit ToggleAllPolyphonicEnvelopeAction(int64_t _moduleId, bool _newState)
                : moduleId(_moduleId)
                , newState(_newState)
            {
                name = "toggle poly/mono on all envelope outputs";
            }

            void undo() override;
            void redo() override;
        };
    }
}
