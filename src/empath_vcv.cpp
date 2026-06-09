// Sapphire Empath for VCV Rack, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <array>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_crossfader.hpp"
#include "sapphire_smoother.hpp"
#include "cascade_filter.hpp"
#include "chaos_fountain.hpp"

namespace Sapphire
{
    namespace Empath
    {
        namespace OutputStage
        {
            // Usually we keep param/port enumerated types with each expander module.
            // This was a special case because I need the InputStage to know output ports,
            // in order to automatically create cables.
            enum OutputId
            {
                AUDIO_LEFT_OUTPUT,
                AUDIO_RIGHT_OUTPUT,
                OUTPUTS_LEN
            };

            struct OutputModule;
            struct OutputWidget;
        }

        constexpr float DefaultLimiterVoltage = 6;

        constexpr int MIN_FILTER_STAGES = 1;
        constexpr int MAX_FILTER_STAGES = 3;
        constexpr int DEFAULT_FILTER_STAGES = 1;

        using filter_t = CascadeFilter<MAX_FILTER_STAGES>;

        struct Frame
        {
            int nchannels = 0;
            std::array<float, PORT_MAX_CHANNELS> sample{};

            void multiply(float factor)
            {
                for (int c = 0; c < nchannels; ++c)
                    sample.at(c) *= factor;
            }

            void invalidate()
            {
                for (float& x : sample)
                    x = NAN;
            }

            void clear()
            {
                for (float& x : sample)
                    x = 0;
            }
        };

        enum class SpectrumDisplayMode
        {
            Monophonic = 0,     // mix of all channels displayed with a single spectrum graph
            Polyphonic = 1,     // each channel gets its own spectrum graph
        };

        struct ChaosFountainInfo
        {
            double dt{};              // time increment in seconds, calculated from speed knob
            float levelKnob{};
            float stereoCrossfade{};  // 0 = mono chaotic CV, 1 = stereo chaotic CV
            float antiClick{};        // 1 most of the time, but ramps down to 0 before, and back up to 1 after changing all chaotic seeds
            bool  reset{};            // set to true only on the specific process() call where all chaos fountains should pick a new random 64-bit chaos seed and regenerate from that new starting position.
            bool  frozen{};           // when true, completely turns off all chaos fountains to reduce CPU usage
            bool  shouldDisplayVoltages{};  // whether or not to show red/green arcs around chaos-influenced attenuverter knobs
        };

        struct ForwardMessage
        {
            bool valid = false;
            int chainIndex = -1;
            bool neonMode{};
            bool polyphonic{};
            Frame dryAudio;         // original input audio from the leftmost module in the chain
            Frame wetAudio;         // cumulative filtered audio from the immediate left module
            Frame cascade;
            int soloCount = 0;      // how many taps have solo enabled
            Frame soloAudio;        // the sum of all output audio for taps with solo enabled
            SpectrumDisplayMode spectrumDisplayMode = SpectrumDisplayMode::Monophonic;
            InterpolatorKind interpolatorKind = InterpolatorKind::Default;
            ChaosFountainInfo chaos;
        };

        struct BackwardMessage
        {
            bool valid = false;
            int soloCount = 0;      // the correct solo count, being reported back from the end of the chain
            float spectrumPowerScale = 1;
        };


        struct ChaosFountainRestoreInfo
        {
            int64_t moduleId;
            uint64_t oldSeed;
            uint64_t newSeed;

            explicit ChaosFountainRestoreInfo(int64_t fountainModuleId, uint64_t fountainSeed)
                : moduleId(fountainModuleId)
                , oldSeed(fountainSeed)
                , newSeed(rack::random::u64())
                {}
        };


        struct RandomizeChaosAction : history::Action
        {
            using list_t = std::vector<ChaosFountainRestoreInfo>;

            list_t list;

            explicit RandomizeChaosAction(const list_t& _list)
                : list(_list)
            {
                name = "randomize chaos generators";
            }

            void undo() override;
            void redo() override;
        };


        struct MoveOutputCablesAction : history::Action
        {
            const int64_t oldModuleId;
            const int64_t newModuleId;
            const int oldPortId;
            const int newPortId;

            explicit MoveOutputCablesAction(PortWidget* oldPort, PortWidget* newPort)
                : oldModuleId(oldPort->module->id)
                , newModuleId(newPort->module->id)
                , oldPortId(oldPort->portId)
                , newPortId(newPort->portId)
            {
                name = "move output cables";
            }

            void undo() override
            {
                moveAllCables(newModuleId, newPortId, oldModuleId, oldPortId);
            }

            void redo() override
            {
                moveAllCables(oldModuleId, oldPortId, newModuleId, newPortId);
            }

        private:
            static PortWidget* findOutputPort(int64_t moduleId, int portId)
            {
                if (ModuleWidget* modWidget = APP->scene->rack->getModule(moduleId))
                    return modWidget->getOutput(portId);
                return nullptr;
            }

            static void moveAllCables(
                int64_t sourceModuleId,
                int     sourcePortId,
                int64_t targetModuleId,
                int     targetPortId
            );
        };


        inline bool IsInput(const Module* module)
        {
            return IsModelType(module, modelSapphireEmpathInput);
        }

        inline bool IsFilter(const Module* module)
        {
            return IsModelType(module, modelSapphireEmpathFilter);
        }

        inline bool IsOutput(const Module* module)
        {
            return IsModelType(module, modelSapphireEmpathOutput);
        }

        inline bool IsOutput(const ModuleWidget* widget)
        {
            return widget && (widget->model == modelSapphireEmpathOutput);
        }

        inline bool IsFilterSender(const Module* module)
        {
            return IsInput(module) || IsFilter(module);
        }

        inline bool IsFilterReceiver(const Module* module)
        {
            return IsFilter(module) || IsOutput(module);
        }

        constexpr int OctaveRange = 5;


        constexpr float ChaosControlVoltage(unsigned channel, float stereoCrossfade, float leftCv, float rightCv)
        {
            if (channel & 1)
            {
                // All odd channels 1, 3, 5, ... 15 are considered "right" channels.
                // So the output can crossfade between left and right on a right channel,
                // whenever the user toggles the chaos mono/stereo button.
                return LinearMix(stereoCrossfade, leftCv, rightCv);
            }

            // Otherwise, we have an even-index channel 0, 2, 4, ... 14.
            // These are all considered "left" channels.
            return leftCv;
        }


        struct EmpathModule : SapphireModule
        {
            int chainIndex = -1;
            float pendingMoveX{};
            int pendingMoveStepCount{};
            ForwardMessage forwardMessageBuffer[2];
            BackwardMessage backwardMessageBuffer[2];
            PortLabelMode outputLabels = PortLabelMode::Stereo;
            bool requestSeedSplash = false;
            uint64_t seedToRestore = 0;
            AutomaticGainLimiter agc;
            bool enableAgc{};
            bool displayChaosVoltagesFlag{};

            explicit EmpathModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
            {
                enableLimiterMenuItems = false;        // Newer style: menu items on knobs only, not panel.
                autoResetVoltageThreshold = 1.0e+6;    // lower values caused too many resets that AGC can handle

                rightExpander.producerMessage = &forwardMessageBuffer[0];
                rightExpander.consumerMessage = &forwardMessageBuffer[1];

                leftExpander.producerMessage = &backwardMessageBuffer[0];
                leftExpander.consumerMessage = &backwardMessageBuffer[1];

                EmpathModule_initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                EmpathModule_initialize();
            }

            void EmpathModule_initialize()
            {
                agc.initialize();
                enableAgc = true;
                displayChaosVoltagesFlag = false;
            }

            double getAgcDistortion() override
            {
                return enableAgc ? (agc.getFollower() - 1.0) : 0.0;
            }

            bool setAgcEnabled(bool enable)
            {
                if (enable && !enableAgc)
                {
                    // If the AGC isn't enabled, and caller wants to enable it,
                    // re-initialize the AGC so it forgets any previous level it had settled on.
                    agc.initialize();
                }
                enableAgc = enable;
                return enable;
            }

            void reflectAgcSlider()
            {
                // Check for changes to the automatic gain control: its level, and whether enabled/disabled.
                if (agcLevelQuantity && agcLevelQuantity->changed)
                {
                    agcLevelQuantity->changed = false;
                    if (setAgcEnabled(agcLevelQuantity->isAgcEnabled()))
                        agc.setCeiling(agcLevelQuantity->clampedAgc());
                }
            }

            ForwardMessage& rightMessageBuffer()
            {
                assert(rightExpander.producerMessage == &forwardMessageBuffer[0] || rightExpander.producerMessage == &forwardMessageBuffer[1]);
                return *static_cast<ForwardMessage*>(rightExpander.producerMessage);
            }

            void sendMessage(const ForwardMessage& message)
            {
                ForwardMessage& destination = rightMessageBuffer();
                destination = message;
                destination.valid = true;
                rightExpander.requestMessageFlip();
            }

            const ForwardMessage* receiveMessage()
            {
                if (IsFilterReceiver(this) && IsFilterSender(leftExpander.module))
                    return static_cast<const ForwardMessage*>(leftExpander.module->rightExpander.consumerMessage);
                return nullptr;
            }

            ForwardMessage receiveMessageOrDefault()
            {
                const ForwardMessage* ptr = receiveMessage();
                return ptr ? *ptr : ForwardMessage{};
            }

            void sendBackwardMessage(const BackwardMessage& backMessage)
            {
                BackwardMessage& destination = *static_cast<BackwardMessage*>(leftExpander.producerMessage);
                destination = backMessage;
                destination.valid = true;
                leftExpander.requestMessageFlip();
            }

            const BackwardMessage* receiveBackwardMessage()
            {
                if (rightExpander.module)
                    return static_cast<const BackwardMessage*>(rightExpander.module->leftExpander.consumerMessage);
                return nullptr;
            }

            BackwardMessage receiveBackwardMessageOrDefault()
            {
                const BackwardMessage* ptr = receiveBackwardMessage();
                return ptr ? *ptr : BackwardMessage{};
            }

            void beginMoveChain(float x)
            {
                pendingMoveX = x;
                pendingMoveStepCount = 2;
            }

            Frame readFrame(int leftInputId, int rightInputId, bool polyphonic, PortLabelMode& mode)
            {
                bool changed = false;   // avoid multi-thread UI label flickering: minimize changes to this->mode.

                Input& inLeft  = inputs.at(leftInputId);
                Input& inRight = inputs.at(rightInputId);

                Frame frame;
                frame.nchannels = 2;
                frame.sample[0] = inLeft.getVoltageSum();
                frame.sample[1] = inRight.getVoltageSum();

                if (!inRight.isConnected())
                {
                    const int ncLeft = inLeft.getChannels();
                    if (ncLeft > 0)
                    {
                        if (ncLeft == 1 || (ncLeft >= 3 && !polyphonic))
                        {
                            // Mono input, so send the same signal to both stereo output channels.
                            frame.sample[1] = frame.sample[0];
                            mode = PortLabelMode::Mono;
                            changed = true;
                        }
                        else if (ncLeft == 2 || polyphonic)
                        {
                            // Polyphonic mode!
                            frame.nchannels = VcvSafeChannelCount(ncLeft);
                            for (int c = 0; c < frame.nchannels; ++c)
                                frame.sample[c] = inLeft.getVoltage(c);
                            mode = static_cast<PortLabelMode>(ncLeft);
                            changed = true;
                        }
                    }
                }

                // Wait until we know we haven't changed the mode to set
                // it to its default value (stereo).
                // I used to do this at the top of the function, and
                // I didn't have a `changed` flag.
                // I kept seeing "2" flickering briefly to "L R" in a particular patch.
                // This makes me believe that the module's call to this function can
                // be pre-empted by a graphics frame update in while mode==stereo.
                // Now the idea is we don't actually change the value in memory until
                // we know for sure that's the value it should have.
                if (!changed)
                    mode = PortLabelMode::Stereo;

                return frame;
            }

            void writeFrame(int leftOutputId, int rightOutputId, const Frame& audio, bool polyphonic)
            {
                Output& audioLeftOutput  = outputs.at(leftOutputId);
                Output& audioRightOutput = outputs.at(rightOutputId);

                if (!polyphonic && audio.nchannels == 2)
                {
                    outputLabels = PortLabelMode::Stereo;

                    audioLeftOutput.setChannels(1);
                    audioLeftOutput.setVoltage(audio.sample[0], 0);

                    audioRightOutput.setChannels(1);
                    audioRightOutput.setVoltage(audio.sample[1], 0);
                }
                else
                {
                    // Polyphonic output to the L port only.

                    outputLabels = static_cast<PortLabelMode>(audio.nchannels);

                    audioLeftOutput.setChannels(audio.nchannels);
                    for (int c = 0; c < audio.nchannels; ++c)
                        audioLeftOutput.setVoltage(audio.sample[c], c);

                    audioRightOutput.setChannels(1);
                    audioRightOutput.setVoltage(0, 0);
                }
            }

            void updateInsertButtonTooltip(int insertButtonParamId)
            {
                std::string text = "Add filter\n";

                if (IsFilter(this))
                {
                    if (IsShiftKeyPressed())
                        text += "SHIFT: default settings\n";
                    else
                        text += "SHIFT: copy settings\n";
                }

                if (IsControlKeyPressed())
                    text += "CTRL: silent output level";
                else
                    text += "CTRL: normal output level";

                updateParamTooltip(insertButtonParamId, text);
            }

            virtual uint64_t getSeed() const = 0;
            virtual void beginSeedChangeAntiClick(uint64_t seed) = 0;

            virtual void silentLevelHook()
            {
            }
        };


        void RandomizeChaosAction::redo()
        {
            for (const ChaosFountainRestoreInfo& node : list)
                if (EmpathModule* empathModule = FindSapphireModule<EmpathModule>(node.moduleId))
                    empathModule->beginSeedChangeAntiClick(node.newSeed);
        }


        void RandomizeChaosAction::undo()
        {
            for (const ChaosFountainRestoreInfo& node : list)
                if (EmpathModule* empathModule = FindSapphireModule<EmpathModule>(node.moduleId))
                    empathModule->beginSeedChangeAntiClick(node.oldSeed);
        }


        struct EmpathWidget : SapphireWidget
        {
            ComponentLocation leftPortLoc;
            ComponentLocation rightPortLoc;

            static ComponentLocation Loc(const std::string& moduleCode, const std::string& prefix, const std::string& suffix)
            {
                return FindComponent(moduleCode, prefix + "_label_" + suffix);
            }

            explicit EmpathWidget(const std::string& moduleCode, const std::string& panelSvgFileName, const std::string& outputPortLabelPrefix)
                : SapphireWidget(moduleCode, panelSvgFileName)
                , leftPortLoc(Loc(moduleCode, outputPortLabelPrefix, "left"))
                , rightPortLoc(Loc(moduleCode, outputPortLabelPrefix, "right"))
                {}

            EmpathModule* filterReceiverWithinRange()
            {
                if (module == nullptr)
                    return nullptr;

                if (Module* right = module->rightExpander.module)
                {
                    if (IsFilterReceiver(right))
                        return dynamic_cast<EmpathModule*>(right);

                    return nullptr;
                }

                // There is no module immediately to the right.
                // Search all modules in the Rack for anything to the right of this module.
                // Pick the module closest to this one, but only if it is within the width
                // we need for the hypothetical filter module we are about to insert.
                const int hpEmpathFilter = hpDistance(PanelWidth("empath_filter"));
                assert(hpEmpathFilter > 0);
                if (ModuleWidget* closestWidget = FindWidgetClosestOnRight(this, hpEmpathFilter))
                    if (IsFilterReceiver(closestWidget->module))
                        return dynamic_cast<EmpathModule*>(closestWidget->module);

                return nullptr;
            }

            void addExpanderInsertButton(int paramId);

            void insertExpander()
            {
                if (module == nullptr)
                    return;

                // If in the middle of a chain, insert another filter.
                // Otherwise terminate the chain with an output module.

                Module* right = filterReceiverWithinRange();
                Model* model = right ? modelSapphireEmpathFilter : modelSapphireEmpathOutput;

                // Erase any obsolete chain indices already in the remaining modules.
                // This prevents them briefly flashing on the screen before being replaced.
                for (Module* m = right; IsFilter(m); m = m->rightExpander.module)
                    if (auto em = dynamic_cast<EmpathModule*>(m))
                        em->chainIndex = -1;

                // Create the expander module.
                bool clone = IsFilter(module) && !IsShiftKeyPressed();
                if (auto emod = AddExpanderModule<EmpathModule>(model, this, ExpanderDirection::Right, clone))
                    if (IsControlKeyPressed())
                        emod->silentLevelHook();
            }

            void removeExpander()
            {
                if (module == nullptr)
                    return;

                // Hand responsibility for moving the rest of the chain to the next module in the chain.
                if (auto nextModule = dynamic_cast<EmpathModule*>(module->rightExpander.module))
                    nextModule->beginMoveChain(box.pos.x);

                // *** DANGER DANGER DANGER ***
                // The following code deletes `this`.
                // Can't do anything more to this object!
                removeAction();
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);
                if (leftPortLoc.cy > 0 && rightPortLoc.cy > 0)
                {
                    auto emod = dynamic_cast<EmpathModule*>(module);
                    PortLabelMode label = emod ? emod->outputLabels : PortLabelMode::Stereo;
                    drawAudioPortLabels(args.vg, label, leftPortLoc.cx, leftPortLoc.cy, rightPortLoc.cx, rightPortLoc.cy);
                }
            }

            void step() override
            {
                SapphireWidget::step();
                pullFromRightWhenNeeded();
                updateBorderVisibility();
                checkSplash();
            }

            void checkSplash()
            {
                if (auto emod = dynamic_cast<EmpathModule*>(module))
                {
                    if (emod->requestSeedSplash)
                    {
                        emod->requestSeedSplash = false;
                        splash.begin(0x80, 0x40, 0x80, 0.1, 0.25);
                    }
                }
            }

            void pullFromRightWhenNeeded()
            {
                auto emod = dynamic_cast<EmpathModule*>(module);
                if (emod && OneShotCountdown(emod->pendingMoveStepCount))
                {
                    // Make a list of all the widgets on the same row, here and to the right.
                    std::vector<EmpathWidget*> widgetsToRight;
                    for (Widget* w : APP->scene->rack->getModuleContainer()->children)
                    {
                        auto other = dynamic_cast<EmpathWidget*>(w);
                        if (other && other->box.pos.y == box.pos.y && other->box.pos.x >= box.pos.x)
                            widgetsToRight.push_back(other);
                    }

                    // Make an ordered list of all the remaining widgets in the chain, before moving anything.
                    std::vector<EmpathWidget*> widgetsInOrder;
                    for (const Module* m = emod; IsFilterReceiver(m); m = m->rightExpander.module)
                    {
                        if (auto otherModule =  dynamic_cast<const EmpathModule*>(m))
                            for (EmpathWidget* otherWidget : widgetsToRight)
                                if (otherWidget->module == otherModule)
                                    widgetsInOrder.push_back(otherWidget);
                    }

                    // Try to move everyone!

                    std::vector<PanelState> panelsInOrder;

                    const float dx = emod->pendingMoveX - box.pos.x;
                    for (auto w : widgetsInOrder)
                    {
                        PanelState s{w};
                        s.newPos = w->box.pos;
                        s.newPos.x += dx;
                        if (APP->scene->rack->requestModulePos(w, s.newPos))
                            panelsInOrder.push_back(s);
                    }

                    if (panelsInOrder.size() > 0)
                    {
                        // When we undo the movement, we have to execute them in reverse order.
                        std::reverse(panelsInOrder.begin(), panelsInOrder.end());

                        // Instead of pushing a separate action onto the history stack,
                        // try to inject the movement as part of the existing history action
                        // that deleted the module.  This is a ComplexAction, as seen in
                        // Rack/src/app/ModuleWidget.cpp, ModuleWidget::removeAction().

                        history::ComplexAction* existingComplexAction = nullptr;
                        if (!APP->history->actions.empty() && APP->history->actionIndex == (int)APP->history->actions.size())
                        {
                            history::Action* oldAction = APP->history->actions.back();
                            existingComplexAction = dynamic_cast<history::ComplexAction*>(oldAction);
                        }

                        auto moveAction = new MoveExpanderAction(panelsInOrder);
                        if (existingComplexAction)
                        {
                            // Good, we can extend the existing chain of actions.
                            existingComplexAction->push(moveAction);
                        }
                        else
                        {
                            // In case something goes wrong, at least push as a separate action.
                            // The user will have to undo twice to undo the deletion, but at least it is possible.
                            APP->history->push(moveAction);
                        }
                    }
                }
            }

            virtual bool isConnectedOnLeft()  const = 0;
            virtual bool isConnectedOnRight() const = 0;

            bool isFilterOnRight() const
            {
                return module && IsFilter(module->rightExpander.module);
            }

            void updateBorderVisibility()
            {
                if (auto emod = dynamic_cast<EmpathModule*>(module))
                {
                    emod->hideLeftBorder  = isConnectedOnLeft();
                    emod->hideRightBorder = isConnectedOnRight();
                }
            }
        };


        using insert_button_base_t = app::SvgSwitch;
        struct InsertButton : insert_button_base_t
        {
            EmpathWidget* empathWidget{};

            explicit InsertButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/right_extender_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };


        using insert_empath_button_base_t = app::SvgSwitch;
        struct InsertAnotherEmpathButton : insert_empath_button_base_t
        {
            OutputStage::OutputWidget* outputWidget{};

            explicit InsertAnotherEmpathButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/right_extender_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };


        using remove_button_base_t = app::SvgSwitch;
        struct RemoveButton : remove_button_base_t
        {
            //**********************************************************************************
            // WARNING: DO NOT try to make RemoveButton derive from SapphireTinyActionButton.
            // It will crash because the remove button deletes its own object!
            // This requires extremely careful coding to avoid corrupting memory.
            // Put simply: it works, so don't fix it!
            // Besides which, this button does not change its representation on the screen,
            // so there is no need to "blink" it in the first place.
            //**********************************************************************************

            EmpathWidget* empathWidget{};

            explicit RemoveButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/remove_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };


        namespace InputStage
        {
            constexpr unsigned nChaoticSignals = 5;
            using fountain_t = ChaosFountain<nChaoticSignals>;
            using batch_t = ChaosBatch<nChaoticSignals>;

            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                OUTPUT_CHANNEL_MODE_BUTTON_PARAM,
                CASCADE_PARAM,
                CASCADE_ATTEN,
                CHAOS_SPEED_PARAM,
                CHAOS_SPEED_ATTEN,
                CHAOS_LEVEL_PARAM,
                CHAOS_LEVEL_ATTEN,
                INIT_CHAIN_BUTTON_PARAM,
                TOGGLE_SPECTRUM_BUTTON_PARAM,
                CHAOS_STEREO_BUTTON_PARAM,
                CHAOS_RANDOMIZE_BUTTON_PARAM,
                INPUT_GAIN_PARAM,
                INPUT_GAIN_ATTEN,
                CHAOS_FREEZE_BUTTON_PARAM,
                CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                AUDIO_LEFT_INPUT,
                AUDIO_RIGHT_INPUT,
                CASCADE_CV_INPUT,
                CHAOS_SPEED_CV_INPUT,
                CHAOS_LEVEL_CV_INPUT,
                INPUT_GAIN_CV_INPUT,
                INPUTS_LEN
            };

            enum OutputId
            {
                OUTPUTS_LEN
            };

            enum LightId
            {
                LIGHTS_LEN
            };

            struct InputWidget;

            struct OutputChannelModeButton : SapphireTinyToggleButton
            {
                InputWidget* inputWidget{};

                explicit OutputChannelModeButton()
                {
                    addTinyButtonFrames(this, "green");
                }
            };

            struct InitChainButton : SapphireTinyActionButton
            {
                InputWidget* inputWidget{};

                explicit InitChainButton()
                {
                    addTinyButtonFrames(this, "green");
                }

                void action() override;
            };

            struct ToggleSpectrumButton : SapphireTinyToggleButton
            {
                explicit ToggleSpectrumButton()
                {
                    addTinyButtonFrames(this, "yellow");
                }
            };

            struct ChaosStereoButton : SapphireTinyToggleButton
            {
                explicit ChaosStereoButton()
                {
                    addTinyButtonFrames(this, "green");
                }
            };

            struct ChaosDisplayVoltagesButton : SapphireTinyToggleButton
            {
                explicit ChaosDisplayVoltagesButton()
                {
                    addTinyButtonFrames(this, "red");
                }
            };

            struct ChaosRandomButton : SapphireTinyActionButton
            {
                InputWidget* inputWidget{};

                explicit ChaosRandomButton()
                {
                    addTinyButtonFrames(this, "red");
                }

                void action() override;
            };


            struct ChaosFreezeButton : SapphireTinyToggleButton
            {
                explicit ChaosFreezeButton()
                {
                    addTinyButtonFrames(this, "xyellow");   // inverted colors: 0=yellow, 1=dark
                }
            };


            struct InputModule : EmpathModule
            {
                PortLabelMode inputLabels = PortLabelMode::Stereo;
                fountain_t fountain{rack::random::u64()};
                Crossfader chaosStereoCrossfader;
                float speedChaos{};
                InterpolatorKind interpolatorKind = InterpolatorKind::Default;
                Smoother chaosAntiClickSmoother{0.025};

                static_assert(std::atomic<unsigned>::is_always_lock_free, "This platform does not support lock-free atomic access, which is required to avoid blocking the audio thread.");
                std::atomic<unsigned> requestedCableCount{0};

                explicit InputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    chainIndex = 0;
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM);
                    configButton(OUTPUT_CHANNEL_MODE_BUTTON_PARAM);
                    configControlGroup("Cascade", CASCADE_PARAM, CASCADE_ATTEN, CASCADE_CV_INPUT, MIN_FILTER_STAGES, MAX_FILTER_STAGES, DEFAULT_FILTER_STAGES);
                    configControlGroup("Chaos speed", CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT, -ChaosOctaveRange, +ChaosOctaveRange);
                    configControlGroup("Chaos level", CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                    configControlGroup("Input gain", INPUT_GAIN_PARAM, INPUT_GAIN_ATTEN, INPUT_GAIN_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                    configStereoInputs(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                    configButton(INIT_CHAIN_BUTTON_PARAM, "Initialize entire chain");
                    configButton(TOGGLE_SPECTRUM_BUTTON_PARAM);
                    configButton(CHAOS_STEREO_BUTTON_PARAM);
                    configButton(CHAOS_RANDOMIZE_BUTTON_PARAM, "Randomize chaotic CV");
                    configButton(CHAOS_FREEZE_BUTTON_PARAM);
                    configButton(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM);
                    attenuverterChaosOptIn(CASCADE_ATTEN);
                    attenuverterChaosOptIn(CHAOS_SPEED_ATTEN);
                    attenuverterChaosOptIn(INPUT_GAIN_ATTEN);
                    InputModule_initialize();
                }

                void InputModule_initialize()
                {
                    chaosStereoCrossfader.setCrossfadeDuration(0.1);
                    chaosStereoCrossfader.snapToFront();
                    speedChaos = 0;
                    chaosAntiClickSmoother.initialize();
                    requestedCableCount.store(0);
                }

                void onReset(const ResetEvent& e) override
                {
                    EmpathModule::onReset(e);
                    InputModule_initialize();
                    fountain.reset();
                }

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    jsonSetEnum(root, "interpolatorKind", interpolatorKind);
                    jsonSaveSeed(root, "chaosFountainSeed", fountain.getSeed());
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    jsonLoadEnum(root, "interpolatorKind", interpolatorKind);
                    if (uint64_t seed = jsonLoadOrGenerateSeed(root, "chaosFountainSeed"))
                        fountain.reset(seed);
                }

                bool isPolyphonicMode()
                {
                    return getParamQuantity(OUTPUT_CHANNEL_MODE_BUTTON_PARAM)->getValue() > 0.5f;
                }

                void beginCableUpdates(bool polyphonic)
                {
                    // This method is called to inform a new Empath input module that it is being created on behalf
                    // of the existing Empath chain on the left, and to please connect cables in series.
                    // If the existing Empath chain is in polyphonic mode, create a single cable and make sure
                    // the new module is also in polyphonic mode. Otherwise, create a pair of stereo cables.
                    params.at(OUTPUT_CHANNEL_MODE_BUTTON_PARAM).setValue(polyphonic ? 1 : 0);

                    // We don't create/move the cables right now. Instead, we set a sentinel value
                    // of 1 or 2 to indicate how many cables to create once expanderCountdown expires.
                    requestedCableCount.store(polyphonic ? 1 : 2);
                }

                SpectrumDisplayMode getSpectrumDisplayMode()
                {
                    float v = getParamQuantity(TOGGLE_SPECTRUM_BUTTON_PARAM)->getValue();
                    return (v > 0.5f) ? SpectrumDisplayMode::Polyphonic : SpectrumDisplayMode::Monophonic;
                }

                Frame readCascade(int nchannels, float chaosLeft, float chaosRight)
                {
                    Frame frame;
                    float cv = 0;
                    frame.nchannels = nchannels;
                    for (int c = 0; c < nchannels; ++c)
                    {
                        nextVoltageOrChaosSignal(cv, CASCADE_CV_INPUT, c, (c&1)?chaosRight:chaosLeft);
                        frame.sample[c] = cvGetControlValue(CASCADE_PARAM, CASCADE_ATTEN, cv, MIN_FILTER_STAGES, MAX_FILTER_STAGES);
                    }
                    return frame;
                }

                float updateStereoCrossfade(float sampleRateHz)
                {
                    float value = getParamQuantity(CHAOS_STEREO_BUTTON_PARAM)->getValue();
                    chaosStereoCrossfader.setTarget(value);
                    return chaosStereoCrossfader.process(sampleRateHz, 0, 1);
                }

                void applyInputGain(Frame& audio, float stereoCrossfade, float chaosL, float chaosR)
                {
                    const int nc = audio.nchannels;
                    float cvInputGain = 0;
                    for (int c = 0; c < nc; ++c)
                    {
                        const float chaos = ChaosControlVoltage(c, stereoCrossfade, chaosL, chaosR);
                        nextVoltageOrChaosSignal(cvInputGain, INPUT_GAIN_CV_INPUT, c, chaos);
                        const float gain = Cube(cvGetControlValue(INPUT_GAIN_PARAM, INPUT_GAIN_ATTEN, cvInputGain, 0, 2));
                        audio.sample[c] *= gain;
                    }
                }

                bool shouldDisplayChaosVoltages() override
                {
                    return params.at(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM).getValue() > 0.5f;
                }

                void process(const ProcessArgs& args) override
                {
                    const double speedKnob = getControlValueChaos(
                        CHAOS_SPEED_PARAM,
                        CHAOS_SPEED_ATTEN,
                        CHAOS_SPEED_CV_INPUT,
                        speedChaos,
                        -ChaosOctaveRange,
                        +ChaosOctaveRange
                    );

                    const bool isChaosLevelZero =
                        (params.at(CHAOS_LEVEL_PARAM).getValue() == 0) &&
                        (params.at(CHAOS_LEVEL_ATTEN).getValue() == 0);

                    const bool isChaosFreezeButtonPressed =
                        (params.at(CHAOS_FREEZE_BUTTON_PARAM).getValue() == 1);

                    ForwardMessage outMessage;
                    outMessage.chainIndex = 1;
                    outMessage.neonMode = neonMode;
                    outMessage.polyphonic = isPolyphonicMode();
                    outMessage.dryAudio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, outMessage.polyphonic, inputLabels);
                    outMessage.wetAudio.nchannels = outMessage.dryAudio.nchannels;
                    outMessage.spectrumDisplayMode = getSpectrumDisplayMode();
                    outMessage.interpolatorKind = interpolatorKind;
                    outMessage.chaos.dt = SimulationTimeIncrement(args.sampleRate, speedKnob);
                    outMessage.chaos.levelKnob = Cube(getControlValueVoltPerOctave(CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT, 0, 2));
                    outMessage.chaos.stereoCrossfade = updateStereoCrossfade(args.sampleRate);
                    outMessage.chaos.shouldDisplayVoltages = shouldDisplayChaosVoltages();
                    outMessage.chaos.frozen = isChaosLevelZero || isChaosFreezeButtonPressed;
                    outMessage.chaos.antiClick = chaosAntiClickSmoother.process(args.sampleRate);
                    outMessage.dryAudio.multiply(outMessage.chaos.antiClick);

                    displayChaosVoltagesFlag = outMessage.chaos.shouldDisplayVoltages;

                    outMessage.chaos.reset = chaosAntiClickSmoother.isDelayedActionReady();
                    if (outMessage.chaos.reset && seedToRestore)
                    {
                        fountain.reset(seedToRestore);
                        seedToRestore = 0;
                    }

                    if (!outMessage.chaos.frozen)
                        fountain.update(outMessage.chaos.dt);

                    const batch_t batch = fountain.getBatch(outMessage.chaos.levelKnob);

                    const float cascadeChaosL = batch.signal.at(0);
                    const float cascadeChaosR = batch.signal.at(1);
                    speedChaos = batch.signal.at(2);
                    const float inputGainChaosL = batch.signal.at(3);
                    const float inputGainChaosR = batch.signal.at(4);

                    reportChaosStereo(CASCADE_ATTEN, outMessage.chaos.stereoCrossfade, cascadeChaosL, cascadeChaosR);
                    reportChaosMono(CHAOS_SPEED_ATTEN, speedChaos);
                    reportChaosStereo(INPUT_GAIN_ATTEN, outMessage.chaos.stereoCrossfade, inputGainChaosL, inputGainChaosR);

                    const float smoothChaosR = LinearMix(
                        outMessage.chaos.stereoCrossfade,
                        cascadeChaosL,
                        cascadeChaosR
                    );

                    outMessage.cascade = readCascade(
                        outMessage.dryAudio.nchannels,
                        cascadeChaosL,
                        smoothChaosR
                    );

                    applyInputGain(
                        outMessage.dryAudio,
                        outMessage.chaos.stereoCrossfade,
                        inputGainChaosL,
                        inputGainChaosR
                    );

                    sendMessage(outMessage);
                }

                uint64_t getSeed() const override
                {
                    return fountain.getSeed();
                }

                void beginSeedChangeAntiClick(uint64_t seed) override
                {
                    seedToRestore = seed;
                    requestSeedSplash = true;
                    chaosAntiClickSmoother.begin();
                }

                void randomizeChaos()
                {
                    // Make a list of (module_id, seed) pairs so later "undo" can restore all the seeds.
                    std::vector<ChaosFountainRestoreInfo> list;
                    const EmpathModule* empathModule = this;
                    do
                    {
                        list.push_back(ChaosFountainRestoreInfo(empathModule->id, empathModule->getSeed()));
                        empathModule = dynamic_cast<const EmpathModule*>(empathModule->rightExpander.module);
                    }
                    while (IsFilterReceiver(empathModule));

                    InvokeAction(new RandomizeChaosAction(list));
                }

                void updateCablesForSerialOperation(const SapphireModule* newOutputModule)
                {
                    if (const unsigned count = requestedCableCount.exchange(0))
                    {
                        if (IsOutput(newOutputModule))
                        {
                            if (Module* oldOutputModule = leftExpander.module; IsOutput(oldOutputModule))
                            {
                                if (auto oldOutputWidget = FindWidgetForId(oldOutputModule->id); IsOutput(oldOutputWidget))
                                {
                                    if (auto newOutputWidget = FindWidgetForId(newOutputModule->id); IsOutput(newOutputWidget))
                                    {
                                        // Move all audio output cables from the old output module
                                        // to the corresponding ports on the new output module.
                                        moveExistingCables(oldOutputWidget, newOutputWidget, OutputStage::AUDIO_LEFT_OUTPUT);
                                        moveExistingCables(oldOutputWidget, newOutputWidget, OutputStage::AUDIO_RIGHT_OUTPUT);

                                        // Create one cable for mono/polyphonic operation, two for stereo.
                                        createCable(AUDIO_LEFT_INPUT, oldOutputModule, OutputStage::AUDIO_LEFT_OUTPUT);
                                        if (count >= 2)
                                            createCable(AUDIO_RIGHT_INPUT, oldOutputModule, OutputStage::AUDIO_RIGHT_OUTPUT);
                                    }
                                }
                            }
                        }
                    }
                }

            private:
                void moveExistingCables(ModuleWidget* oldOutputWidget, ModuleWidget* newOutputWidget, int outputPortId)
                {
                    if (oldOutputWidget && newOutputWidget)
                        if (PortWidget* oldPort = oldOutputWidget->getOutput(outputPortId))
                            if (PortWidget* newPort = newOutputWidget->getOutput(outputPortId))
                                InvokeAction(new MoveOutputCablesAction(oldPort, newPort));
                }

                void createCable(int inputId, Module* outputModule, int outputId)
                {
                    auto cable = new Cable;
                    cable->inputModule = this;
                    cable->inputId = inputId;
                    cable->outputModule = outputModule;
                    cable->outputId = outputId;
                    APP->engine->addCable(cable);

                    auto cw = new CableWidget;
                    cw->setCable(cable);
                    cw->color = settings::cableColors.at(0);
                    APP->scene->rack->addCable(cw);

                    auto hist = new history::CableAdd;
                    hist->setCable(cw);
                    APP->history->push(hist);
                }
            };

            struct InputWidget : EmpathWidget
            {
                InputModule* inputModule{};
                int expanderCountdown = 8;

                explicit InputWidget(InputModule* module)
                    : EmpathWidget("empath_input", asset::plugin(pluginInstance, "res/empath_input.svg"), "")
                    , inputModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(INSERT_BUTTON_PARAM);
                    addStereoInputPorts(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                    addOutputChannelModeButton();
                    addSapphireControlGroup("cascade", CASCADE_PARAM, CASCADE_ATTEN, CASCADE_CV_INPUT);
                    addSnapVoctFlatControlGroup("cspeed", CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT);
                    addSapphireFlatControlGroup("clevel", CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT);
                    addInitChainButton();
                    addToggleSpectrumButton();
                    addChaosStereoButton();
                    addChaosRandomButton();
                    addChaosFreezeButton();
                    addChaosDisplayVoltagesButton();
                    addSapphireFlatControlGroup("input_gain", INPUT_GAIN_PARAM, INPUT_GAIN_ATTEN, INPUT_GAIN_CV_INPUT, 3.0, 3.5);
                }

                bool isConnectedOnLeft() const override
                {
                    return false;
                }

                bool isConnectedOnRight() const override
                {
                    return module && IsFilterReceiver(module->rightExpander.module);
                }

                void addOutputChannelModeButton()
                {
                    auto button = createParamCentered<OutputChannelModeButton>(Vec{}, inputModule, OUTPUT_CHANNEL_MODE_BUTTON_PARAM);
                    button->inputWidget = this;
                    addSapphireParam(button, "channel_mode_button");
                }

                void addInitChainButton()
                {
                    auto button = createParamCentered<InitChainButton>(Vec{}, inputModule, INIT_CHAIN_BUTTON_PARAM);
                    button->inputWidget = this;
                    addSapphireParam(button, "init_chain_button");
                }

                void addToggleSpectrumButton()
                {
                    auto button = createParamCentered<ToggleSpectrumButton>(Vec{}, inputModule, TOGGLE_SPECTRUM_BUTTON_PARAM);
                    addSapphireParam(button, "toggle_spectrum_button");
                }

                void addChaosStereoButton()
                {
                    auto button = createParamCentered<ChaosStereoButton>(Vec{}, inputModule, CHAOS_STEREO_BUTTON_PARAM);
                    addSapphireParam(button, "chaos_stereo_button");
                }

                void addChaosRandomButton()
                {
                    auto button = createParamCentered<ChaosRandomButton>(Vec{}, inputModule, CHAOS_RANDOMIZE_BUTTON_PARAM);
                    button->inputWidget = this;
                    addSapphireParam(button, "chaos_random_button");
                }

                void addChaosFreezeButton()
                {
                    auto button = createParamCentered<ChaosFreezeButton>(Vec{}, inputModule, CHAOS_FREEZE_BUTTON_PARAM);
                    addSapphireParam(button, "chaos_freeze_button");
                }

                void addChaosDisplayVoltagesButton()
                {
                    auto button = createParamCentered<ChaosDisplayVoltagesButton>(Vec{}, inputModule, CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM);
                    addSapphireParam(button, "chaos_display_button");
                }

                void drawSpectrumConnectorLine(NVGcontext* vg)
                {
                    if (isFilterOnRight())
                    {
                        constexpr float buttonRadius = 4.5;
                        ComponentLocation mmpos = FindComponent(modcode, "toggle_spectrum_button");
                        float bx = mm2px(mmpos.cx);
                        float by = mm2px(mmpos.cy);

                        nvgBeginPath(vg);
                        nvgStrokeColor(vg, SCHEME_BLACK);
                        nvgStrokeWidth(vg, 0.75);
                        nvgMoveTo(vg, bx + buttonRadius, by);
                        nvgLineTo(vg, box.size.x + DX_REMOVE_GAP, by);
                        nvgStroke(vg);
                    }
                }

                void draw(const DrawArgs& args) override
                {
                    EmpathWidget::draw(args);
                    PortLabelMode label = inputModule ? inputModule->inputLabels : PortLabelMode::Stereo;
                    ComponentLocation L = FindComponent(modcode, "input_label_left");
                    ComponentLocation R = FindComponent(modcode, "input_label_right");
                    drawAudioPortLabels(args.vg, label, L.cx, L.cy, R.cx, R.cy);
                    drawSpectrumConnectorLine(args.vg);
                }

                void step() override
                {
                    EmpathWidget::step();
                    if (inputModule)
                    {
                        // When we first add the input module, it needs to automatically create
                        // an output module, then insert a filter between.
                        // But prevent auto-creation when reloading the patch later.
                        if (OneShotCountdown(expanderCountdown))
                        {
                            if (!IsFilterReceiver(module->rightExpander.module) && !APP->history->canRedo())
                            {
                                if (SapphireModule* newOutputModule = AddExpander(modelSapphireEmpathOutput, this, ExpanderDirection::Right, false))
                                {
                                    AddExpander(modelSapphireEmpathFilter, this, ExpanderDirection::Right, false);
                                    inputModule->updateCablesForSerialOperation(newOutputModule);
                                }
                            }
                        }

                        // Keep the button's hovertext in sync with its actual state.
                        inputModule->updateToggleButtonTooltip(OUTPUT_CHANNEL_MODE_BUTTON_PARAM, "Stereo mode", "Polyphonic mode");
                        inputModule->updateToggleButtonTooltip(CHAOS_STEREO_BUTTON_PARAM, "Chaos CV: MONO", "Chaos CV: STEREO");
                        inputModule->updateToggleButtonTooltip(CHAOS_FREEZE_BUTTON_PARAM, "Chaos engine: RUNNING", "Chaos engine: STOPPED");
                        inputModule->updateToggleButtonTooltip(TOGGLE_SPECTRUM_BUTTON_PARAM, "Spectrum graph: MONO", "Spectrum graph: POLYPHONIC");
                        inputModule->updateToggleButtonTooltip(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM, "Display chaos voltages: NO", "Display chaos voltages: YES");
                        inputModule->updateInsertButtonTooltip(INSERT_BUTTON_PARAM);
                    }
                }

                void appendContextMenu(Menu* menu) override
                {
                    EmpathWidget::appendContextMenu(menu);
                    if (inputModule)
                    {
                        menu->addChild(new MenuSeparator);
                        menu->addChild(CreateChangeEnumMenuItem(
                            "Interpolator",
                            {
                                "Linear (uses less CPU)",
                                "Sinc (cleaner audio)"
                            },
                            "change interpolator",
                            inputModule->interpolatorKind
                        ));
                    }
                }

                void initializeExpanderChain()
                {
                    if (inputModule)
                    {
                        // We need to create a history item for the undo/redo stack.
                        // This item has to remember the json-serialized form of each module
                        // before we reset it. We do not need to remember the reset state, because
                        // we can always reset again!

                        std::vector<InitChainNode> list;
                        list.push_back(InitChainNode(inputModule));
                        APP->engine->resetModule(inputModule);

                        Module* module = inputModule->rightExpander.module;
                        while (IsFilterReceiver(module))
                        {
                            list.push_back(InitChainNode(module));
                            APP->engine->resetModule(module);
                            module = module->rightExpander.module;
                        }

                        APP->history->push(new InitChainAction(list));
                    }
                }

                void randomizeChaos()
                {
                    if (inputModule)
                        inputModule->randomizeChaos();
                }
            };


            void InitChainButton::action()
            {
                if (inputWidget)
                    inputWidget->initializeExpanderChain();
            }


            void ChaosRandomButton::action()
            {
                if (inputWidget)
                    inputWidget->randomizeChaos();
            }
        };

        //----------------------------------------------------------------------------

        namespace FilterStage
        {
            struct FilterModule;
            struct FilterWidget;

            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                REMOVE_BUTTON_PARAM,
                FREQ_PARAM,
                FREQ_ATTEN,
                RES_PARAM,
                RES_ATTEN,
                PAN_PARAM,
                PAN_ATTEN,
                MODE_BUTTON_PARAM,
                LEVEL_PARAM,
                LEVEL_ATTEN,
                MUTE_BUTTON_PARAM,
                SOLO_BUTTON_PARAM,
                ENV_GAIN_PARAM,
                INIT_FILTER_BUTTON_PARAM,
                AGC_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                FREQ_CV_INPUT,
                RES_CV_INPUT,
                PAN_CV_INPUT,
                LEVEL_CV_INPUT,
                _OBSOLETE_INPUT_1,
                AUDIO_LEFT_INPUT,       // return L
                AUDIO_RIGHT_INPUT,      // return R
                INPUTS_LEN
            };

            enum OutputId
            {
                AUDIO_LEFT_OUTPUT,      // send L
                AUDIO_RIGHT_OUTPUT,     // send R
                ENV_OUTPUT,
                OUTPUTS_LEN
            };

            enum LightId
            {
                MODE_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct ChannelInfo
            {
                filter_t filter;

                void initialize()
                {
                    filter.initialize();
                }
            };

            enum class FilterMode
            {
                Bandpass,
                Notch,
            };

            enum class FilterSource
            {
                Dry,
                Wet,
            };


            struct MuteButton : app::SvgSwitch
            {
                explicit MuteButton()
                {
                    momentary = false;
                    addTinyButtonFrames(this, "red");
                }
            };


            struct SoloButton : app::SvgSwitch
            {
                explicit SoloButton()
                {
                    momentary = false;
                    addTinyButtonFrames(this, "green");
                }
            };


            struct InitFilterButton : SapphireTinyActionButton
            {
                FilterWidget* filterWidget{};

                explicit InitFilterButton()
                {
                    addTinyButtonFrames(this, "green");
                }

                void action() override;
            };


            struct FilterModeButton : SapphireTinyToggleButton
            {
                explicit FilterModeButton()
                {
                    addTinyButtonFrames(this, "yellow");
                }
            };


            constexpr unsigned SpectrumBits = 12;
            constexpr unsigned SpectrumLength = 1 << SpectrumBits;
            using fft_delay_line_t = DelayLine<float, SpectrumLength>;


            struct SpectrumWidget : OpaqueWidget
            {
                static constexpr unsigned fdenom = 4;
                static constexpr unsigned niter = SpectrumLength / fdenom;
                static_assert(niter > 0);

                FilterModule* filterModule{};
                unsigned nchannels{};
                unsigned prev_nchannels{};
                std::array<fft_delay_line_t, PORT_MAX_CHANNELS> fftDelayLines;
                std::vector<float> fftBufferIn;
                std::vector<float> fftBufferOut;
                std::array<std::vector<float>, PORT_MAX_CHANNELS> vuMeter;
                bool vuReset{};
                rack::dsp::RealFFT fftEngine{SpectrumLength};
                SpectrumDisplayMode displayMode = SpectrumDisplayMode::Monophonic;
                SpectrumDisplayMode prevDisplayMode = SpectrumDisplayMode::Monophonic;
                float powerScale = 1;

                explicit SpectrumWidget(FilterModule* _filterModule)
                    : filterModule(_filterModule)
                {
                    ComponentLocation upperLeft  = FindComponent("empath_filter", "spectrum_upper_left");
                    ComponentLocation lowerRight = FindComponent("empath_filter", "spectrum_lower_right");
                    box.pos.x = mm2px(upperLeft.cx);
                    box.pos.y = mm2px(upperLeft.cy);
                    box.size.x = mm2px(lowerRight.cx - upperLeft.cx);
                    box.size.y = mm2px(lowerRight.cy - upperLeft.cy);
                    fftBufferIn.resize(SpectrumLength);
                    fftBufferOut.resize(SpectrumLength);
                    for (auto& v : vuMeter)
                        v.resize(niter);
                    initialize();
                }

                void onRemove(const RemoveEvent&) override;

                void initialize()
                {
                    displayMode = prevDisplayMode = SpectrumDisplayMode::Monophonic;
                    prev_nchannels = 0;
                    vuReset = true;

                    for (fft_delay_line_t& delayLine : fftDelayLines)
                        delayLine.clear();
                }

                void draw(const DrawArgs& args) override
                {
                    drawBlackRectangle(args);
                    OpaqueWidget::draw(args);   // in case we ever have children to draw on top
                }

                void drawLayer(const DrawArgs& args, int layer) override;

                void drawBlackRectangle(const DrawArgs& args)
                {
                    math::Rect r = box.zeroPos();
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, RECT_ARGS(r));
                    nvgFillColor(args.vg, SCHEME_BLACK);
                    nvgFill(args.vg);
                }

                NVGcolor barColor(float mix) const;

                void graphSpectrum(const DrawArgs& args, unsigned c, unsigned nc)
                {
                    if (nc == 0)
                        return;     // There is nothing to draw. Prevent division by zero.

                    if (nc > PORT_MAX_CHANNELS)
                        return;     // Invalid number of channels. Prevent bad memory access.

                    if (c >= nc)
                        return;     // Ignore any out-of-bounds channel. Prevent bad memory access.

                    // The filter module's delay line is a circular buffer.
                    // Copy and re-align the data to start at fftBufferIn[0].
                    // Multiply by a Hann window function to eliminate frequency spikes
                    // from edge discontinuities.

                    constexpr float factor = M_PI / (SpectrumLength-1);
                    for (unsigned offset = 0; offset < SpectrumLength; ++offset)
                    {
                        float& y = fftBufferIn[offset];
                        if (displayMode == SpectrumDisplayMode::Monophonic)
                        {
                            // Produce a monophonic voltage from the average of all channel voltages.
                            y = 0;
                            for (unsigned k=0; k < nchannels; ++k)
                                y += fftDelayLines[k].readForward(offset);
                            y /= nc;
                        }
                        else
                        {
                            // Produce a polyphonic voltage from each individual channel.
                            y = fftDelayLines[c].readForward(offset);
                        }
                        y *= Square(std::sin(factor * offset));   // Hann window function
                    }

                    // Take the real-valued Fast Fourier Transform.
                    fftEngine.rfft(fftBufferIn.data(), fftBufferOut.data());

                    // Eliminate the nyquist-frequency value at index 1,
                    // because we don't want to include it in the
                    // DC power value at index 0.
                    // See rack::dsp::RealFFT::rfft() documentation for
                    // explanation of the frequency bin representation.
                    fftBufferOut.at(1) = 0;

                    // Graph the data in fftBufferOut.
                    // horizontal axis = linear frequency
                    // vertical axis = dB power, with tanh compression on top of that.
                    // Each channel of nc=1..16 possible channels needs an equal amount
                    // of vertical space.
                    // In mono mode, nc==1, but nchannels can be any value 1..16.
                    // So we divide by nc to indicate how many bands to split vertical space into.
                    float dyPerChannel = box.size.y / nc;
                    const unsigned cflip = (nc-1) - c;                 // hack to reverse channel order: 0 at top, not bottom
                    float yBase = box.size.y - cflip*dyPerChannel;
                    float yMiddle = yBase - dyPerChannel/2;

                    constexpr float dbShift = -3.2;
                    constexpr float vuSettle = 0.11;

                    constexpr float nf = niter;
                    constexpr float strokeWidthPx = fdenom / 8.0;
                    const float dxPerFreqBin = box.size.x / niter;

                    unsigned col = 0;
                    float vuPrev{};
                    for (unsigned f=0; f+1 < niter; f+=2)
                    {
                        float x = dxPerFreqBin * f;
                        float power = std::max(1.0e-6f, Square(fftBufferOut.at(f)) + Square(fftBufferOut.at(f+1)));
                        float& vu = vuMeter[c][col];
                        float db = powerScale*(std::log10(power) + dbShift);
                        float dyPowerPx = (dyPerChannel/2) * std::tanh(db);
                        float yTop = yMiddle - dyPowerPx;   // array of these values for each column. quick rise, slow fade.

                        if (vuReset)
                            vu = yBase;
                        else if (yTop < vu)
                            vu = yTop;
                        else
                            vu = LinearMix(vuSettle, vu, yTop);

                        NVGcolor riserColor = barColor(f/nf);
                        NVGcolor segmentColor = FadeColor(0.25, 1, riserColor, SCHEME_WHITE);

                        nvgBeginPath(args.vg);
                        nvgLineCap(args.vg, NVG_BUTT);
                        nvgStrokeWidth(args.vg, strokeWidthPx);
                        nvgStrokeColor(args.vg, riserColor);
                        nvgMoveTo(args.vg, x, yBase);
                        nvgLineTo(args.vg, x, yTop);
                        nvgStroke(args.vg);

                        if (f > 0)
                        {
                            nvgBeginPath(args.vg);
                            nvgStrokeWidth(args.vg, 0.4);
                            nvgStrokeColor(args.vg, segmentColor);
                            nvgMoveTo(args.vg, x - 2*dxPerFreqBin, vuPrev);
                            nvgLineTo(args.vg, x, vu);
                            nvgStroke(args.vg);
                        }

                        vuPrev = vu;
                        ++col;
                    }
                }
            };


            constexpr unsigned nChaoticSignals = 7;
            using fountain_t = ChaosFountain<nChaoticSignals>;
            using batch_t = ChaosBatch<nChaoticSignals>;


            struct FilterModule : EmpathModule
            {
                ChannelInfo channel[PORT_MAX_CHANNELS];
                Crossfader modeFader;       // front=bandpass, back=notch
                Crossfader muteFader;       // front=normal,   back=muted
                Crossfader soloFader;       // front=normal,   back=solo
                int totalSoloCount = 0;     // the total number of solo-enabled filters in this chain
                fountain_t fountain{rack::random::u64()};
                SpectrumWidget* spectrum{};

                explicit FilterModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    shouldOfferFireDrill = true;
                    enableEnvelopeFollower();
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM);
                    configButton(REMOVE_BUTTON_PARAM, "Remove filter");
                    configControlGroup(
                        "Frequency",
                        FREQ_PARAM,
                        FREQ_ATTEN,
                        FREQ_CV_INPUT,
                        -OctaveRange,
                        +OctaveRange,
                        0,
                        " Hz",
                        2,
                        C4_FREQUENCY_HZ     // match VCV VCO frequency
                    );
                    configControlGroup("Resonance", RES_PARAM, RES_ATTEN, RES_CV_INPUT, 0, 1, 0);
                    configControlGroup("Panning", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT, -1, +1, 0, "%", 0, 100);
                    configControlGroup("Level", LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT, 0, 1, 1, " dB", -10, 20);
                    configStereoInputs(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "return");
                    configStereoOutputs(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, "send");
                    configButton(MUTE_BUTTON_PARAM);            // tooltip changed dynamically
                    configButton(SOLO_BUTTON_PARAM);            // tooltip changed dynamically
                    configOutput(ENV_OUTPUT, "Envelope follower");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    configButton(INIT_FILTER_BUTTON_PARAM, "Initialize this filter only");
                    addAgcLevelQuantity(AGC_PARAM, 1, DefaultLimiterVoltage);
                    attenuverterChaosOptIn(FREQ_ATTEN);
                    attenuverterChaosOptIn(RES_ATTEN);
                    attenuverterChaosOptIn(LEVEL_ATTEN);
                    attenuverterChaosOptIn(PAN_ATTEN);
                    FilterModule_initialize();
                }

                void FilterModule_initialize()
                {
                    clearAudio();
                    modeFader.snapToFront();
                    muteFader.snapToFront();
                    soloFader.snapToFront();
                    totalSoloCount = 0;
                    if (spectrum)
                        spectrum->initialize();
                }

                void onReset(const ResetEvent& e) override
                {
                    EmpathModule::onReset(e);
                    FilterModule_initialize();
                    if (fountain.getSeed())
                        fountain.reset();
                }

                void clearAudio()
                {
                    envelopeFollower.initialize();
                    for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                        channel[c].initialize();
                }

                bool isBadOutput(const Frame& frame) const
                {
                    return SapphireModule::isBadOutput(frame.sample.data(), frame.nchannels);
                }

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    jsonSaveSeed(root, "chaosFountainSeed", fountain.getSeed());
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    if (uint64_t seed = jsonLoadOrGenerateSeed(root, "chaosFountainSeed"))
                        fountain.reset(seed);
                }

                bool isAudible() const
                {
                    // If other filter(s) enable solo, but not this filter, then this filter is silent.
                    if (totalSoloCount>0 && !soloFader.atBack())
                        return false;

                    // If we are not muted, we are audible.
                    return !muteFader.atBack();
                }

                bool isModeNotch()
                {
                    return params.at(MODE_BUTTON_PARAM).getValue() > 0.5f;
                }

                bool isSoloEnabled()
                {
                    return params.at(SOLO_BUTTON_PARAM).getValue() > 0.5f;
                }

                FilterMode updateFilterMode()
                {
                    bool notch = isModeNotch();
                    FilterMode mode = notch ? FilterMode::Notch : FilterMode::Bandpass;
                    setLightBrightness(MODE_BUTTON_LIGHT, notch);
                    return mode;
                }

                Frame panFrame(const Frame& rawAudio, float chaos)
                {
                    Frame pannedAudio = rawAudio;
                    if (const int nc = pannedAudio.nchannels; nc >= 2)
                    {
                        constexpr float sensitivity = 1.0 / 5.0;        // 1 knob unit per 5V at 100%
                        const float x = getControlValueChaos(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT, chaos, -1, +1, sensitivity);
                        const PanningFactors pf = Panning(x);

                        // Support panning for all pairs of supplied channels.
                        // Any remaining odd channel will be left as-is.
                        for (int c=0; c+1 < nc; c+=2)
                        {
                            pannedAudio.sample[c+0] *= pf.left;
                            pannedAudio.sample[c+1] *= pf.right;
                        }
                    }
                    return pannedAudio;
                }

                void updateTooltips()
                {
                    updateToggleButtonTooltip(MUTE_BUTTON_PARAM, "Mute: OFF", "Mute: ON");
                    updateToggleButtonTooltip(SOLO_BUTTON_PARAM, "Solo: OFF", "Solo: ON");

                    const std::string name = isModeNotch() ? "Morph" : "Resonance";
                    updateParamTooltip(RES_PARAM, name);
                    updateParamTooltip(RES_ATTEN, name + " attenuverter");
                    updateInputTooltip(RES_CV_INPUT, name + " CV");

                    updateInsertButtonTooltip(INSERT_BUTTON_PARAM);
                }

                int updateSolo(Frame& soloFrame, const Frame& inFrame, float sampleRateHz)
                {
                    soloFrame.nchannels = inFrame.nchannels;
                    soloFader.setTarget(isSoloEnabled());
                    float factor = soloFader.process(sampleRateHz, 0, 1);
                    if (factor > 0)
                    {
                        for (int c=0; c < inFrame.nchannels; ++c)
                            soloFrame.sample[c] += factor * inFrame.sample[c];
                        return 1;
                    }
                    return 0;
                }

                float updateMuteState(float sampleRateHz)
                {
                    muteFader.setTarget(params.at(MUTE_BUTTON_PARAM).getValue() > 0.5f);
                    return muteFader.process(sampleRateHz, 1, 0);
                }

                void postInsertFilterHook() override
                {
                    fountain.reset(rack::random::u64());
                }


                void silentLevelHook() override
                {
                    params.at(LEVEL_PARAM).setValue(0);
                }


                uint64_t getSeed() const override
                {
                    return fountain.getSeed();
                }

                void beginSeedChangeAntiClick(uint64_t seed) override
                {
                    seedToRestore = seed;
                    requestSeedSplash = true;
                }

                void process(const ProcessArgs& args) override
                {
                    const ForwardMessage inMessage = receiveMessageOrDefault();
                    ForwardMessage outMessage = inMessage;

                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();
                    totalSoloCount = inBackMessage.soloCount;

                    chainIndex = inMessage.chainIndex;
                    displayChaosVoltagesFlag = inMessage.chaos.shouldDisplayVoltages;

                    if (inMessage.chainIndex > 0)
                        outMessage.chainIndex = 1 + inMessage.chainIndex;

                    const FilterMode mode = updateFilterMode();
                    modeFader.setTarget(mode == FilterMode::Notch);
                    const float modeMix = modeFader.process(args.sampleRate, 0, 1);     // 0=bandpass, 1=notch
                    const float muteFactor = updateMuteState(args.sampleRate);

                    if (inMessage.chaos.reset && seedToRestore)
                    {
                        fountain.reset(seedToRestore);
                        seedToRestore = 0;
                    }

                    if (!inMessage.chaos.frozen)
                        fountain.update(inMessage.chaos.dt);

                    const batch_t batch = fountain.getBatch(inMessage.chaos.levelKnob);

                    const float freqChaosL  = batch.signal.at(0);
                    const float freqChaosR  = batch.signal.at(1);
                    const float resChaosL   = batch.signal.at(2);
                    const float resChaosR   = batch.signal.at(3);
                    const float levelChaosL = batch.signal.at(4);
                    const float levelChaosR = batch.signal.at(5);
                    const float panChaos    = batch.signal.at(6);

                    // The "report" calls are for updating voltage colors around the attenuverter knobs.
                    reportChaosStereo(FREQ_ATTEN,  inMessage.chaos.stereoCrossfade, freqChaosL,  freqChaosR);
                    reportChaosStereo(RES_ATTEN,   inMessage.chaos.stereoCrossfade, resChaosL,   resChaosR);
                    reportChaosStereo(LEVEL_ATTEN, inMessage.chaos.stereoCrossfade, levelChaosL, levelChaosR);
                    reportChaosMono(PAN_ATTEN, panChaos);

                    Frame sendFrame;    // audio sent to the SEND ports
                    Frame returnFrame;  // audio received back from the RTRN ports or normalled from sendFrame

                    if (spectrum)
                        spectrum->nchannels = 0;    // blank the graph unless we find data below

                    if (inMessage.valid)
                    {
                        Frame levelFrame;

                        const int nc = inMessage.dryAudio.nchannels;
                        sendFrame.nchannels = nc;
                        levelFrame.nchannels = nc;
                        returnFrame.nchannels = nc;
                        if (spectrum)
                        {
                            spectrum->nchannels = nc;
                            spectrum->displayMode = inMessage.spectrumDisplayMode;
                            spectrum->powerScale = inBackMessage.spectrumPowerScale;
                        }

                        float cvFreq = 0;
                        float cvRes = 0;
                        float cvLevel = 0;

                        for (int c = 0; c < nc; ++c)
                        {
                            auto& q = channel[c];
                            if (limiterRecoveryCountdown > 0)
                            {
                                sendFrame.sample[c] = 0;
                                returnFrame.sample[c] = 0;
                                levelFrame.sample[c] = 0;
                            }
                            else
                            {
                                float freqChaos  = ChaosControlVoltage(c, inMessage.chaos.stereoCrossfade, freqChaosL,  freqChaosR);
                                float resChaos   = ChaosControlVoltage(c, inMessage.chaos.stereoCrossfade, resChaosL,   resChaosR);
                                float levelChaos = ChaosControlVoltage(c, inMessage.chaos.stereoCrossfade, levelChaosL, levelChaosR);

                                nextVoltageOrChaosSignal(cvFreq, FREQ_CV_INPUT, c, freqChaos);
                                nextVoltageOrChaosSignal(cvRes, RES_CV_INPUT, c, resChaos);
                                nextVoltageOrChaosSignal(cvLevel, LEVEL_CV_INPUT, c, levelChaos);

                                float freqKnob  = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, cvFreq, -OctaveRange, +OctaveRange);
                                float resKnob   = cvGetControlValue(RES_PARAM, RES_ATTEN, cvRes);
                                float levelKnob = cvGetControlValue(LEVEL_PARAM, LEVEL_ATTEN, cvLevel, 0, 1);

                                q.filter.setFrequency(freqKnob);
                                q.filter.setResonance(resKnob);
                                q.filter.setInterpolator(inMessage.interpolatorKind);

                                sendFrame.sample[c] = inMessage.chaos.antiClick * q.filter.process(
                                    args.sampleRate,
                                    inMessage.dryAudio.sample[c],
                                    inMessage.cascade.sample[c],
                                    modeMix
                                );

                                returnFrame.sample[c] = readSample(
                                    sendFrame.sample[c],
                                    AUDIO_LEFT_INPUT,
                                    AUDIO_RIGHT_INPUT,
                                    c
                                );

                                levelFrame.sample[c] =
                                    inMessage.chaos.antiClick *
                                    levelKnob *
                                    muteFactor *
                                    returnFrame.sample[c];
                            }

                            if (spectrum)
                                spectrum->fftDelayLines[c].write(sendFrame.sample[c]);
                        }

                        Frame outputFrame = panFrame(levelFrame, panChaos);

                        reflectAgcSlider();
                        if (enableAgc)
                            agc.process(args.sampleRate, outputFrame.nchannels, outputFrame.sample.data());

                        outMessage.soloCount += updateSolo(outMessage.soloAudio, outputFrame, args.sampleRate);
                        for (int c = 0; c < outMessage.wetAudio.nchannels; ++c)
                            outMessage.wetAudio.sample[c] += outputFrame.sample[c];
                    }

                    if (isFireDrillOneShot())
                    {
                        // Simulate that something in the filter got messed up and all the math went to NAN.
                        // This helps test the filter module's reaction to an actual math failure.
                        sendFrame.invalidate();
                    }

                    if (isBadOutput(sendFrame) || isBadOutput(returnFrame) || isBadOutput(outMessage.wetAudio))
                    {
                        sendFrame.clear();
                        returnFrame.clear();
                        outMessage.wetAudio.clear();
                        clearAudio();
                        beginRecovery(args.sampleRate);
                    }

                    writeFrame(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, sendFrame, inMessage.polyphonic);
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, returnFrame.nchannels, returnFrame.sample.data());

                    // Keep 'neon mode' unified along the entire expander chain.
                    includeNeonModeMenuItem = !inMessage.valid;
                    if (inMessage.valid)
                        neonMode = inMessage.neonMode;

                    // Either create or copy the backward message.
                    BackwardMessage outBackMessage;
                    if (inBackMessage.valid)
                    {
                        // We received a valid backward-message from the module to the right of us.
                        outBackMessage = inBackMessage;
                    }
                    else
                    {
                        // This module is the rightmost of the expander chain currently.
                        // Therefore, we become the source-of-truth for all backward-traveling information.
                        outBackMessage.soloCount = outMessage.soloCount;
                    }

                    sendMessage(outMessage);
                    sendBackwardMessage(outBackMessage);

                    if (limiterRecoveryCountdown > 0)
                        --limiterRecoveryCountdown;     // one less sample of silence before filter comes back online
                }

                static float blend(const float* stageSample, float cascade)
                {
                    if (cascade >= MAX_FILTER_STAGES)
                        return stageSample[MAX_FILTER_STAGES];

                    if (cascade <= 0)
                        return stageSample[0];

                    unsigned index = std::floor(cascade);
                    float frac = cascade - index;
                    return (1-frac)*stageSample[index] + frac*stageSample[index+1];
                }

                bool shouldDisplayChaosVoltages() override
                {
                    return displayChaosVoltagesFlag;
                }
            };


            struct FilterWidget : EmpathWidget
            {
                FilterModule* filterModule{};
                const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");
                SvgOverlay* resLabel{};
                SvgOverlay* morphLabel{};

                explicit FilterWidget(FilterModule* module)
                    : EmpathWidget("empath_filter", asset::plugin(pluginInstance, "res/empath_filter.svg"), "sendreturn")
                    , filterModule(module)
                {
                    setModule(module);
                    addResMorphLabels();
                    addEnvelopeFollowerLabels();
                    addExpanderInsertButton(INSERT_BUTTON_PARAM);
                    addExpanderRemoveButton(REMOVE_BUTTON_PARAM);
                    addInitFilterButton();
                    addFilterModeButton();
                    addSnapVoctFlatControlGroup("freq", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
                    addSapphireFlatControlGroup("res", RES_PARAM, RES_ATTEN, RES_CV_INPUT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroupWithWarningLight("level", LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT);
                    addStereoInputPorts(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "return");
                    addStereoOutputPorts(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, "send");
                    addMuteSoloButtons();
                    addSapphireOutput<EnvelopeOutputPort>(ENV_OUTPUT, "env_output");
                    addSmallKnob(ENV_GAIN_PARAM, "env_gain_knob");
                    addSpectrumWidget();
                }

                void addSpectrumWidget()
                {
                    auto spectrum = new SpectrumWidget(filterModule);
                    if (filterModule)
                        filterModule->spectrum = spectrum;
                    addChild(spectrum);
                }

                void addResMorphLabels()
                {
                    resLabel = addLabelOverlay("res/empath_label_res.svg", true);
                    morphLabel = addLabelOverlay("res/empath_label_morph.svg", false);
                }

                void addMuteSoloButtons()
                {
                    auto muteButton = createParamCentered<MuteButton>(Vec{}, module, MUTE_BUTTON_PARAM);
                    addSapphireParam(muteButton, "mute_button");

                    auto soloButton = createParamCentered<SoloButton>(Vec{}, module, SOLO_BUTTON_PARAM);
                    addSapphireParam(soloButton, "solo_button");
                }


                void addFilterModeButton()
                {
                    auto button = createParamCentered<FilterModeButton>(Vec{}, module, MODE_BUTTON_PARAM);
                    addSapphireParam(button, "mode_button");
                }


                void addExpanderRemoveButton(int paramId)
                {
                    auto button = createParamCentered<RemoveButton>(Vec{}, filterModule, paramId);
                    button->empathWidget = this;
                    addSapphireParam(button, "remove_button");
                }

                void addInitFilterButton()
                {
                    auto button = createParamCentered<InitFilterButton>(Vec{}, filterModule, INIT_FILTER_BUTTON_PARAM);
                    button->filterWidget = this;
                    addSapphireParam(button, "init_filter_button");
                }

                bool isConnectedOnLeft() const override
                {
                    return module && IsFilterSender(module->leftExpander.module);
                }

                bool isConnectedOnRight() const override
                {
                    return module && IsFilterReceiver(module->rightExpander.module);
                }

                void step() override
                {
                    EmpathWidget::step();
                    updateModeButton();
                    updateResLabel();
                    updateEnvDuck();
                    if (filterModule)
                    {
                        filterModule->updateTooltips();
                    }
                }

                void updateModeButton()
                {
                    if (filterModule)
                    {
                        const bool isBandpass = filterModule->modeFader.atFront();

                        filterModule->paramQuantities.at(MODE_BUTTON_PARAM)->name =
                            std::string("Mode: ") + (isBandpass ? "BANDPASS" : "NOTCH/COMB");
                    }
                }

                void updateResLabel()
                {
                    const bool showMorph = filterModule && filterModule->isModeNotch();
                    resLabel->setVisible(!showMorph);
                    morphLabel->setVisible(showMorph);
                }

                void draw(const DrawArgs& args) override
                {
                    EmpathWidget::draw(args);
                    if (filterModule)
                    {
                        if (!filterModule->neonMode)
                            drawChainIndex(args.vg, filterModule->chainIndex, nvgRGB(0x66, 0x06, 0x5c));

                        if (!filterModule->isAudible())
                            drawMuteIndicator(args.vg);

                        drawSpectrumConnectorLine(args.vg);
                    }
                }

                void drawLayer(const DrawArgs& args, int layer) override
                {
                    EmpathWidget::drawLayer(args, layer);
                    if (layer==1 && filterModule)
                    {
                        if (filterModule->neonMode)
                            drawChainIndex(args.vg, filterModule->chainIndex, getNeonColor());
                    }
                }

                void drawSpectrumConnectorLine(NVGcontext* vg)
                {
                    if (filterModule && filterModule->spectrum)
                    {
                        bool left  = isConnectedOnLeft();
                        bool right = isConnectedOnRight();
                        if (left || right)
                        {
                            const Vec& pos  = filterModule->spectrum->box.pos;
                            const Vec& size = filterModule->spectrum->box.size;
                            float y = pos.y + size.y/2;

                            nvgBeginPath(vg);
                            if (left)
                            {
                                nvgMoveTo(vg, -DX_REMOVE_GAP, y);
                                nvgLineTo(vg, pos.x, y);
                            }
                            if (right)
                            {
                                nvgMoveTo(vg, pos.x + size.x, y);
                                nvgLineTo(vg, box.size.x + DX_REMOVE_GAP, y);
                            }
                            nvgStrokeColor(vg, SCHEME_BLACK);
                            nvgStrokeWidth(vg, 0.75);
                            nvgStroke(vg);
                        }
                    }
                }

                Vec getChainIndexCenterPos() const
                {
                    float yCenter = mm2px(4.5);
                    float xCenter = box.size.x/2;
                    return Vec(xCenter, yCenter);
                }

                void drawChainIndex(
                    NVGcontext* vg,
                    int chainIndex,
                    NVGcolor textColor)
                {
                    if (module == nullptr)
                        return;

                    if (auto font = APP->window->loadFont(chainFontPath))
                    {
                        char text[20];

                        nvgFontSize(vg, 18);
                        nvgFontFaceId(vg, font->handle);
                        nvgFillColor(vg, textColor);

                        if (chainIndex > 0)
                        {
                            snprintf(text, sizeof(text), "%d", chainIndex);
                            Vec center = getChainIndexCenterPos();
                            drawCenteredText(vg, center.x, center.y, text);
                        }
                    }
                }
            };


            void InitFilterButton::action()
            {
                if (filterWidget)
                    filterWidget->resetAction();
            }


            void SpectrumWidget::onRemove(const RemoveEvent &e)
            {
                if (filterModule)
                {
                    filterModule->spectrum = nullptr;
                    filterModule = nullptr;
                }
                OpaqueWidget::onRemove(e);
            }

            void SpectrumWidget::drawLayer(const DrawArgs &args, int layer)
            {
                if (layer==1 && filterModule && nchannels>0 && nchannels<=PORT_MAX_CHANNELS)
                {
                    if (prev_nchannels != nchannels || prevDisplayMode != displayMode)
                    {
                        prev_nchannels = nchannels;
                        prevDisplayMode = displayMode;
                        vuReset = true;
                    }

                    if (displayMode == SpectrumDisplayMode::Monophonic)
                    {
                        graphSpectrum(args, 0, 1);
                    }
                    else
                    {
                        for (unsigned c = 0; c < nchannels; ++c)
                            graphSpectrum(args, c, nchannels);
                    }
                    vuReset = false;
                }
            }

            NVGcolor SpectrumWidget::barColor(float mix) const
            {
                NVGcolor mutedColor = nvgRGB(0x20, 0x40, 0x50);
                if (filterModule && !filterModule->isAudible())
                    return mutedColor;

                constexpr unsigned nGuideColors = 5;

                // These RGB color values were derived using the online calculator at:
                // https://colornamer.robertcooper.me/

                const std::array<NVGcolor,nGuideColors> rainbow
                {
                    nvgRGB(0xff, 0x00, 0x00),   // 0 "red"
                    nvgRGB(0xff, 0xa5, 0x00),   // 1 "orange"
                    nvgRGB(0xff, 0xff, 0x00),   // 2 "yellow"
                    nvgRGB(0x00, 0x80, 0x00),   // 3 "green"
                    nvgRGB(0x00, 0x10, 0xff),   // 4 "blue"
                    // I don't have indigo or violet because they don't look like a real rainbow.
                };

                const float hue = mix * (nGuideColors-1);
                const unsigned namedColor = std::clamp<unsigned>(hue, 0, nGuideColors-2);
                const float residue = hue - namedColor;
                return FadeColor(residue, 1, rainbow.at(namedColor), rainbow.at(namedColor+1));
            }
        }

        //----------------------------------------------------------------------------

        namespace OutputStage
        {
            constexpr unsigned nChaoticSignals = 4;
            using fountain_t = ChaosFountain<nChaoticSignals>;
            using batch_t = ChaosBatch<nChaoticSignals>;

            enum ParamId
            {
                GLOBAL_MIX_PARAM,
                GLOBAL_MIX_ATTEN,
                GLOBAL_LEVEL_PARAM,
                GLOBAL_LEVEL_ATTEN,
                SPECTRUM_VERTICAL_SCALE_PARAM,
                AGC_PARAM,
                INSERT_EMPATH_BUTTON,  // Button to add another Empath expander chain in series with this one.
                PARAMS_LEN
            };

            enum InputId
            {
                GLOBAL_MIX_CV_INPUT,
                GLOBAL_LEVEL_CV_INPUT,
                INPUTS_LEN
            };

            // ****
            // enum OutputId
            // ... is toward top of file so it can be used to create cables from InputStage.
            // ****

            enum LightId
            {
                LIGHTS_LEN
            };

            struct OutputModule : EmpathModule
            {
                Crossfader firstSoloFader;      // crossfades the treansition between muting everyone else or not
                fountain_t fountain{rack::random::u64()};
                bool isPolyphonicPortMode{};

                explicit OutputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configStereoOutputs(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, "audio");
                    configControlGroup("Output mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT, 0, 1, 1, "%", 0, 100);
                    configControlGroup("Output level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                    configParam(SPECTRUM_VERTICAL_SCALE_PARAM, -1, +1, 0, "Vertical scale");
                    addAgcLevelQuantity(AGC_PARAM, 1, DefaultLimiterVoltage);
                    configButton(INSERT_EMPATH_BUTTON, "Add another Empath chain in series");
                    attenuverterChaosOptIn(GLOBAL_MIX_ATTEN);
                    attenuverterChaosOptIn(GLOBAL_LEVEL_ATTEN);
                    OutputModule_initialize();
                }

                void OutputModule_initialize()
                {
                    firstSoloFader.snapToFront();
                }

                void onReset(const ResetEvent& e) override
                {
                    EmpathModule::onReset(e);
                    OutputModule_initialize();
                    fountain.reset();
                }

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    jsonSaveSeed(root, "chaosFountainSeed", fountain.getSeed());
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    if (uint64_t seed = jsonLoadOrGenerateSeed(root, "chaosFountainSeed"))
                        fountain.reset(seed);
                }

                void beginSeedChangeAntiClick(uint64_t seed) override
                {
                    seedToRestore = seed;
                    requestSeedSplash = true;
                }

                uint64_t getSeed() const override
                {
                    return fountain.getSeed();
                }

                float spectrumPower()
                {
                    float knob = params.at(SPECTRUM_VERTICAL_SCALE_PARAM).getValue();
                    return TenToPower<float>(0.65 * knob);
                }

                void process(const ProcessArgs& args) override
                {
                    BackwardMessage backMessage;
                    const ForwardMessage inMessage = receiveMessageOrDefault();
                    isPolyphonicPortMode = inMessage.polyphonic;
                    chainIndex = inMessage.chainIndex;
                    displayChaosVoltagesFlag = inMessage.chaos.shouldDisplayVoltages;
                    backMessage.soloCount = inMessage.soloCount;
                    backMessage.spectrumPowerScale = spectrumPower();

                    includeNeonModeMenuItem = !inMessage.valid;
                    if (inMessage.valid)
                        neonMode = inMessage.neonMode;

                    firstSoloFader.setTarget(inMessage.soloCount > 0);
                    float solo = firstSoloFader.process(args.sampleRate, 0, 1);

                    if (inMessage.chaos.reset && seedToRestore)
                    {
                        fountain.reset(seedToRestore);
                        seedToRestore = 0;
                    }

                    if (!inMessage.chaos.frozen)
                        fountain.update(inMessage.chaos.dt);

                    const batch_t batch = fountain.getBatch(inMessage.chaos.levelKnob);

                    Frame audio = outputAudioFrame(
                        inMessage.dryAudio,
                        inMessage.wetAudio,
                        inMessage.soloAudio,
                        inMessage.chaos.stereoCrossfade,
                        inMessage.chaos.antiClick,
                        solo,
                        batch
                    );

                    reflectAgcSlider();
                    if (enableAgc)
                        agc.process(args.sampleRate, audio.nchannels, audio.sample.data());

                    writeFrame(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, audio, inMessage.polyphonic);
                    sendBackwardMessage(backMessage);
                }

                Frame outputAudioFrame(
                    const Frame& dryAudio,
                    const Frame& wetAudio,
                    const Frame& soloAudio,
                    float chaosStereoCrossfade,
                    float chaosAntiClick,
                    float soloFactor,
                    const batch_t& chaosBatch)
                {
                    const float mixChaosL   = chaosBatch.signal.at(0);
                    const float mixChaosR   = chaosBatch.signal.at(1);
                    const float levelChaosL = chaosBatch.signal.at(2);
                    const float levelChaosR = chaosBatch.signal.at(3);

                    reportChaosStereo(GLOBAL_MIX_ATTEN, chaosStereoCrossfade, mixChaosL, mixChaosR);
                    reportChaosStereo(GLOBAL_LEVEL_ATTEN, chaosStereoCrossfade, levelChaosL, levelChaosR);

                    constexpr float gainSensitivity = 1.0 / 5.0;    // one knob unit per 5V change in CV
                    float cvMix = 0;
                    float cvLevel = 0;
                    const int nc = dryAudio.nchannels;
                    Frame result;
                    result.nchannels = nc;
                    for (int c = 0; c < nc; ++c)
                    {
                        float mixChaos = ChaosControlVoltage(c, chaosStereoCrossfade, mixChaosL, mixChaosR);
                        float levelChaos = ChaosControlVoltage(c, chaosStereoCrossfade, levelChaosL, levelChaosR);
                        nextVoltageOrChaosSignal(cvMix, GLOBAL_MIX_CV_INPUT, c, mixChaos);
                        nextVoltageOrChaosSignal(cvLevel, GLOBAL_LEVEL_CV_INPUT, c, levelChaos);
                        float level = Cube(cvGetVoltPerOctave(GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, cvLevel * gainSensitivity, 0, 2));
                        float mix = filter_t::filter_t::MixFactor(cvGetControlValue(GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, cvMix, 0, 1));
                        float wet = LinearMix(soloFactor, wetAudio.sample[c], soloAudio.sample[c]);
                        result.sample[c] = chaosAntiClick * level * LinearMix(mix, dryAudio.sample[c], wet);
                    }
                    return result;
                }

                bool shouldDisplayChaosVoltages() override
                {
                    return displayChaosVoltagesFlag;
                }
            };


            struct OutputWidget : EmpathWidget
            {
                OutputModule* outputModule{};

                explicit OutputWidget(OutputModule* module)
                    : EmpathWidget("empath_output", asset::plugin(pluginInstance, "res/empath_output.svg"), "output")
                    , outputModule(module)
                {
                    setModule(module);
                    addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                    addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                    addSapphireControlGroup("global_mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT);
                    addSapphireControlGroupWithWarningLight("global_level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT);
                    addKnob<Trimpot>(SPECTRUM_VERTICAL_SCALE_PARAM, "spectrum_vertical_scale");
                    addInsertAnotherEmpathButton();
                }

                bool isConnectedOnLeft() const override
                {
                    return module && IsFilterSender(module->leftExpander.module);
                }

                bool isConnectedOnRight() const override
                {
                    return false;
                }

                void draw(const DrawArgs& args) override
                {
                    EmpathWidget::draw(args);
                    drawSpectrumConnectorLine(args.vg);
                }

                void drawSpectrumConnectorLine(NVGcontext* vg)
                {
                    if (outputModule)
                    {
                        if (auto leftModule = dynamic_cast<FilterStage::FilterModule*>(outputModule->leftExpander.module))
                        {
                            if (leftModule->spectrum)
                            {
                                const Vec& pos  = leftModule->spectrum->box.pos;
                                const Vec& size = leftModule->spectrum->box.size;
                                const float y = pos.y + size.y/2;
                                const float x1 = mm2px(0.0) - DX_REMOVE_GAP;
                                const float x2 = mm2px(2.0);

                                nvgBeginPath(vg);
                                nvgMoveTo(vg, x1, y);
                                nvgLineTo(vg, x2, y);
                                nvgStrokeColor(vg, SCHEME_BLACK);
                                nvgStrokeWidth(vg, 0.75);
                                nvgLineCap(vg, NVG_BUTT);
                                nvgStroke(vg);
                            }
                        }
                    }
                }

                void addInsertAnotherEmpathButton();

                void insertAnotherEmpath()
                {
                    if (outputModule)
                        if (auto newEmpathInputModule = AddExpanderModule<InputStage::InputModule>(modelSapphireEmpathInput, this, ExpanderDirection::Right, false))
                            newEmpathInputModule->beginCableUpdates(outputModule->isPolyphonicPortMode);
                }
            };
        }

        void InsertButton::onButton(const event::Button& e)
        {
            insert_button_base_t::onButton(e);
            if (empathWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    empathWidget->insertExpander();
            }
        }


        void RemoveButton::onButton(const event::Button& e)
        {
            remove_button_base_t::onButton(e);
            if (empathWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    empathWidget->removeExpander();
            }
        }

        void InsertAnotherEmpathButton::onButton(const event::Button &e)
        {
            insert_empath_button_base_t::onButton(e);
            if (outputWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    outputWidget->insertAnotherEmpath();
            }
        }

        void EmpathWidget::addExpanderInsertButton(int paramId)
        {
            auto button = createParamCentered<InsertButton>(Vec{}, module, paramId);
            button->empathWidget = this;
            addSapphireParam(button, "insert_button");
        }

        namespace OutputStage
        {
            void OutputWidget::addInsertAnotherEmpathButton()
            {
                auto button = createParamCentered<InsertAnotherEmpathButton>(Vec{}, module, INSERT_EMPATH_BUTTON);
                button->outputWidget = this;
                addSapphireParam(button, "insert_empath_button");
            }
        }


        void MoveOutputCablesAction::moveAllCables(
            int64_t sourceModuleId,
            int     sourcePortId,
            int64_t targetModuleId,
            int     targetPortId)
        {
            if (PortWidget* sourcePort = findOutputPort(sourceModuleId, sourcePortId))
            {
                if (PortWidget* targetPort = findOutputPort(targetModuleId, targetPortId))
                {
                    std::vector<CableWidget*> cableList = APP->scene->rack->getCablesOnPort(sourcePort);
                    for (CableWidget* cable : cableList)
                    {
                        cable->outputPort = targetPort;
                        cable->updateCable();
                    }
                }
            }
        }
    }
}

Model* modelSapphireEmpathInput = createSapphireModel<Sapphire::Empath::InputStage::InputModule, Sapphire::Empath::InputStage::InputWidget>(
    "Empath",
    Sapphire::ExpanderRole::None
);

Model* modelSapphireEmpathFilter = createSapphireModel<Sapphire::Empath::FilterStage::FilterModule, Sapphire::Empath::FilterStage::FilterWidget>(
    "EmpathFilter",
    Sapphire::ExpanderRole::None
);

Model* modelSapphireEmpathOutput = createSapphireModel<Sapphire::Empath::OutputStage::OutputModule, Sapphire::Empath::OutputStage::OutputWidget>(
    "EmpathOutput",
    Sapphire::ExpanderRole::None
);
