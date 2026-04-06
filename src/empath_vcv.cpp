#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_crossfader.hpp"
#include "cascade_filter.hpp"
#include "chaos_fountain.hpp"

namespace Sapphire
{
    namespace Empath
    {
        constexpr int MIN_FILTER_STAGES = 1;
        constexpr int MAX_FILTER_STAGES = 3;
        constexpr int DEFAULT_FILTER_STAGES = 1;

        using filter_t = CascadeFilter<MAX_FILTER_STAGES>;

        struct Frame
        {
            int nchannels = 0;
            float sample[PORT_MAX_CHANNELS]{};
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
            float chaosSpeedKnob{};
            float chaosLevelKnob{};
        };

        struct BackwardMessage
        {
            bool valid = false;
            int soloCount = 0;      // the correct solo count, being reported back from the end of the chain
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

        inline bool IsFilterSender(const Module* module)
        {
            return IsInput(module) || IsFilter(module);
        }

        inline bool IsFilterReceiver(const Module* module)
        {
            return IsFilter(module) || IsOutput(module);
        }

        constexpr int OctaveRange = 5;

        struct EmpathModule : SapphireModule
        {
            int chainIndex = -1;
            float pendingMoveX{};
            int pendingMoveStepCount{};
            ForwardMessage forwardMessageBuffer[2];
            BackwardMessage backwardMessageBuffer[2];
            PortLabelMode outputLabels = PortLabelMode::Stereo;

            explicit EmpathModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
            {
                rightExpander.producerMessage = &forwardMessageBuffer[0];
                rightExpander.consumerMessage = &forwardMessageBuffer[1];

                leftExpander.producerMessage = &backwardMessageBuffer[0];
                leftExpander.consumerMessage = &backwardMessageBuffer[1];
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
                mode = PortLabelMode::Stereo;

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
                        }
                        else if (ncLeft == 2 || polyphonic)
                        {
                            // Polyphonic mode!
                            frame.nchannels = VcvSafeChannelCount(ncLeft);
                            for (int c = 0; c < frame.nchannels; ++c)
                                frame.sample[c] = inLeft.getVoltage(c);
                            mode = static_cast<PortLabelMode>(ncLeft);
                        }
                    }
                }

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
        };


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
                const int hpEchoTap = hpDistance(PanelWidth("empath_filter"));
                assert(hpEchoTap > 0);
                if (ModuleWidget* closestWidget = FindWidgetClosestOnRight(this, hpEchoTap))
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
                AddExpander(model, this, ExpanderDirection::Right, true);
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


        namespace EInput
        {
            constexpr unsigned nChaoticSignals = 1;
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
                PARAMS_LEN
            };

            enum InputId
            {
                AUDIO_LEFT_INPUT,
                AUDIO_RIGHT_INPUT,
                CASCADE_CV_INPUT,
                CHAOS_SPEED_CV_INPUT,
                CHAOS_LEVEL_CV_INPUT,
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

            struct InputModule : EmpathModule
            {
                bool autoCreateExpanders = true;
                PortLabelMode inputLabels = PortLabelMode::Stereo;
                fountain_t fountain;

                explicit InputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    chainIndex = 0;
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Add filter");
                    configButton(OUTPUT_CHANNEL_MODE_BUTTON_PARAM);     // dynamic tooltip / hovertext
                    configControlGroup("Cascade", CASCADE_PARAM, CASCADE_ATTEN, CASCADE_CV_INPUT, MIN_FILTER_STAGES, MAX_FILTER_STAGES, DEFAULT_FILTER_STAGES);
                    configControlGroup("Chaos speed", CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT, -ChaosOctaveRange, +ChaosOctaveRange);
                    configControlGroup("Chaos level", CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                    configStereoInputs(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                    configButton(INIT_CHAIN_BUTTON_PARAM, "Initialize entire chain");
                    InputModule_initialize();
                }

                void InputModule_initialize()
                {
                }

                void onReset(const ResetEvent& e) override
                {
                    EmpathModule::onReset(e);
                    InputModule_initialize();
                    if (fountain.getSeed())
                        fountain.reset();
                }

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    jsonSetBool(root, "autoCreateExpanders", autoCreateExpanders);
                    json_object_set_new(root, "chaosFountainSeed", json_integer(fountain.getSeed()));
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    jsonLoadBool(root, "autoCreateExpanders", autoCreateExpanders);
                    if (json_t* jseed = json_object_get(root, "chaosFountainSeed"); json_is_integer(jseed))
                        fountain.reset(json_integer_value(jseed));
                }

                bool polyphonicMode()
                {
                    return getParamQuantity(OUTPUT_CHANNEL_MODE_BUTTON_PARAM)->getValue() > 0.5f;
                }

                Frame readCascade(int nchannels, float chaos)
                {
                    Frame frame;
                    float cv = 0;
                    frame.nchannels = nchannels;
                    for (int c = 0; c < nchannels; ++c)
                    {
                        nextVoltageOrChaosSignal(cv, CASCADE_CV_INPUT, c, chaos);
                        frame.sample[c] = cvGetControlValue(CASCADE_PARAM, CASCADE_ATTEN, cv, MIN_FILTER_STAGES, MAX_FILTER_STAGES);
                    }
                    return frame;
                }

                void process(const ProcessArgs& args) override
                {
                    ForwardMessage outMessage;
                    outMessage.chainIndex = 1;
                    outMessage.neonMode = neonMode;
                    outMessage.polyphonic = polyphonicMode();
                    outMessage.dryAudio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, outMessage.polyphonic, inputLabels);
                    outMessage.wetAudio.nchannels = outMessage.dryAudio.nchannels;
                    outMessage.chaosSpeedKnob = getControlValue(CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT, -ChaosOctaveRange, +ChaosOctaveRange);
                    outMessage.chaosLevelKnob = Cube(getControlValueVoltPerOctave(CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT, 0, 2));

                    const batch_t batch = fountain.process(
                        args.sampleRate,
                        outMessage.chaosSpeedKnob,
                        outMessage.chaosLevelKnob
                    );

                    const float cascadeChaos = batch.signal.at(0);
                    outMessage.cascade = readCascade(outMessage.dryAudio.nchannels, cascadeChaos);

                    sendMessage(outMessage);
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
                    addSapphireFlatControlGroup("cspeed", CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT);
                    addSapphireFlatControlGroup("clevel", CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT);
                    addInitChainButton();
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

                void draw(const DrawArgs& args) override
                {
                    EmpathWidget::draw(args);
                    PortLabelMode label = inputModule ? inputModule->inputLabels : PortLabelMode::Stereo;
                    ComponentLocation L = FindComponent(modcode, "input_label_left");
                    ComponentLocation R = FindComponent(modcode, "input_label_right");
                    drawAudioPortLabels(args.vg, label, L.cx, L.cy, R.cx, R.cy);
                }

                void step() override
                {
                    EmpathWidget::step();
                    if (inputModule)
                    {
                        // When we first add the input module, it needs to automatically create
                        // an output module, then insert a filter between.
                        // But prevent auto-creation when reloading the patch later.
                        if (inputModule->autoCreateExpanders && OneShotCountdown(expanderCountdown))
                        {
                            inputModule->autoCreateExpanders = false;   // prevent automatic creation when loading the patch
                            if (!IsFilterReceiver(module->rightExpander.module) && !APP->history->canRedo())
                            {
                                AddExpander(modelSapphireEmpathOutput, this, ExpanderDirection::Right, false);
                                AddExpander(modelSapphireEmpathFilter, this, ExpanderDirection::Right, false);
                            }
                        }

                        // Keep the button's hovertext in sync with its actual state.
                        inputModule->updateToggleButtonTooltip(OUTPUT_CHANNEL_MODE_BUTTON_PARAM, "Stereo mode", "Polyphonic mode");
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
            };


            void InitChainButton::action()
            {
                if (inputWidget)
                    inputWidget->initializeExpanderChain();
            }
        };

        //----------------------------------------------------------------------------

        namespace Filter
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


            constexpr unsigned SpectrumBits = 9;
            constexpr unsigned SpectrumLength = 1 << SpectrumBits;
            using fft_delay_line_t = DelayLine<float, SpectrumLength>;


            struct SpectrumWidget : OpaqueWidget
            {
                FilterModule* filterModule{};
                unsigned nchannels{};
                std::array<fft_delay_line_t, PORT_MAX_CHANNELS> fftDelayLines;
                std::vector<float> fftBufferIn;
                std::vector<float> fftBufferOut;
                rack::dsp::RealFFT fftEngine{SpectrumLength};

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
                    initialize();
                }

                void onRemove(const RemoveEvent&) override;

                void initialize()
                {
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

                void graphSpectrum(const DrawArgs& args, unsigned c)
                {
                    // The filter module's delay line is a circular buffer.
                    // Copy and re-align the data to start at fftBuffer[0].
                    // Multiply by a Hann window function to eliminate frequency spikes
                    // from edge discontinuities.

                    float factor = M_PI / (SpectrumLength-1);
                    for (unsigned offset = 0; offset < SpectrumLength; ++offset)
                    {
                        float w = Square(std::sin(factor * offset));
                        fftBufferIn[offset] = w * fftDelayLines[c].readForward(offset);
                    }

                    // Take the real-valued Fast Fourier Transform.
                    fftEngine.rfft(fftBufferIn.data(), fftBufferOut.data());

                    // Graph the data in fftBufferOut.
                    // horizontal axis = linear frequency
                    // vertical axis = dB power
                    // Each channel of 1..16 possible channels needs an equal amount
                    // of vertical space.
                    float dyPerChannel = box.size.y / nchannels;
                    float yBase = box.size.y - c*dyPerChannel;     // subtract to make channels go upward from the bottom
                    float yMiddle = yBase - dyPerChannel/2;
                    float dxPerFreqBin = box.size.x / SpectrumLength;

                    static float dbMin = +9999;
                    static float dbMax = -9999;
                    static float prevMin = 0;
                    static float prevMax = 0;

                    constexpr float strokeWidthPx = 0.5;
                    for (unsigned f = 0; f+1 < SpectrumLength; f += 2)
                    {
                        float x = dxPerFreqBin * f;
                        float power = std::hypotf(fftBufferOut.at(f), fftBufferOut.at(f+1));
                        if (power > 0)
                        {
                            float db = std::log(power);
                            dbMin = std::min(dbMin, db);
                            dbMax = std::min(dbMax, db);
                            float dyPowerPx = (dyPerChannel/2) * std::tanh(db);
                            float yTop = yMiddle - dyPowerPx;

                            nvgBeginPath(args.vg);
                            nvgLineCap(args.vg, NVG_BUTT);
                            nvgStrokeWidth(args.vg, strokeWidthPx);
                            nvgStrokeColor(args.vg, nvgRGB(0xf5, 0xbc, 0x42));
                            nvgMoveTo(args.vg, x, yBase);
                            nvgLineTo(args.vg, x, yTop);
                            nvgStroke(args.vg);
                        }
                    }

                    if (dbMin != prevMin || dbMax != prevMax)
                    {
                        INFO("dbMin=%g, dbMax=%g", dbMin, dbMax);
                        prevMin = dbMin;
                        prevMax = dbMax;
                    }
                }
            };


            constexpr unsigned nChaoticSignals = 4;
            using fountain_t = ChaosFountain<nChaoticSignals>;
            using batch_t = ChaosBatch<nChaoticSignals>;


            struct FilterModule : EmpathModule
            {
                ChannelInfo channel[PORT_MAX_CHANNELS];
                Crossfader modeFader;       // front=bandpass, back=notch
                Crossfader muteFader;       // front=normal,   back=muted
                Crossfader soloFader;       // front=normal,   back=solo
                int totalSoloCount = 0;     // the total number of solo-enabled filters in this chain
                fountain_t fountain;
                SpectrumWidget* spectrum{};

                explicit FilterModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    enableEnvelopeFollower();
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Add filter");
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
                    FilterModule_initialize();
                }

                void FilterModule_initialize()
                {
                    for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                        channel[c].initialize();

                    modeFader.snapToFront();
                    muteFader.snapToFront();
                    soloFader.snapToFront();
                    totalSoloCount = 0;
                    envelopeFollower.initialize();

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

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    json_object_set_new(root, "chaosFountainSeed", json_integer(fountain.getSeed()));
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    if (json_t* jseed = json_object_get(root, "chaosFountainSeed"); json_is_integer(jseed))
                        fountain.reset(json_integer_value(jseed));
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

                void updateMuteSoloControls()
                {
                    updateToggleButtonTooltip(MUTE_BUTTON_PARAM, "Mute: OFF", "Mute: ON");
                    updateToggleButtonTooltip(SOLO_BUTTON_PARAM, "Solo: OFF", "Solo: ON");
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

                void postCloneHook() override
                {
                    // After cloning one FilterModule to create another (using json),
                    // we have also copied over the seed value. We want each FilterModule
                    // to have its own seed. Invalidate the seed to force the cloned module
                    // to pick a new seed and regenerate the chaotic oscillators.
                    fountain.forgetSeed();
                }

                void process(const ProcessArgs& args) override
                {
                    const ForwardMessage inMessage = receiveMessageOrDefault();
                    ForwardMessage outMessage = inMessage;

                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();
                    totalSoloCount = inBackMessage.soloCount;

                    chainIndex = inMessage.chainIndex;

                    if (inMessage.chainIndex > 0)
                        outMessage.chainIndex = 1 + inMessage.chainIndex;

                    const FilterMode mode = updateFilterMode();
                    modeFader.setTarget(mode == FilterMode::Notch);
                    const float modeMix = modeFader.process(args.sampleRate, 0, 1);     // 0=bandpass, 1=notch
                    const float muteFactor = updateMuteState(args.sampleRate);

                    const batch_t batch = fountain.process(
                        args.sampleRate,
                        inMessage.chaosSpeedKnob,
                        inMessage.chaosLevelKnob
                    );

                    const float freqChaos  = batch.signal.at(0);
                    const float resChaos   = batch.signal.at(1);
                    const float levelChaos = batch.signal.at(2);
                    const float panChaos   = batch.signal.at(3);

                    Frame sendFrame;
                    Frame envelopeFrame;

                    if (spectrum)
                        spectrum->nchannels = 0;    // blank the graph unless we find data below

                    if (inMessage.valid)
                    {
                        Frame levelFrame;

                        const int nc = inMessage.dryAudio.nchannels;
                        sendFrame.nchannels = nc;
                        levelFrame.nchannels = nc;
                        envelopeFrame.nchannels = nc;
                        if (spectrum)
                            spectrum->nchannels = nc;

                        float cvFreq = 0;
                        float cvRes = 0;
                        float cvLevel = 0;

                        for (int c = 0; c < nc; ++c)
                        {
                            auto& q = channel[c];

                            nextVoltageOrChaosSignal(cvFreq, FREQ_CV_INPUT, c, freqChaos);
                            nextVoltageOrChaosSignal(cvRes, RES_CV_INPUT, c, resChaos);
                            nextVoltageOrChaosSignal(cvLevel, LEVEL_CV_INPUT, c, levelChaos);

                            float freqKnob = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, cvFreq, -OctaveRange, +OctaveRange);
                            float resKnob = cvGetControlValue(RES_PARAM, RES_ATTEN, cvRes);
                            float levelKnob = cvGetControlValue(LEVEL_PARAM, LEVEL_ATTEN, cvLevel, 0, 1);

                            q.filter.setFrequency(freqKnob);
                            q.filter.setResonance(resKnob);

                            sendFrame.sample[c] = q.filter.process(
                                args.sampleRate,
                                inMessage.dryAudio.sample[c],
                                inMessage.cascade.sample[c],
                                modeMix
                            );

                            if (spectrum)
                                spectrum->fftDelayLines[c].write(sendFrame.sample[c]);

                            envelopeFrame.sample[c] = readSample(
                                sendFrame.sample[c],
                                AUDIO_LEFT_INPUT,
                                AUDIO_RIGHT_INPUT,
                                c
                            );
                            levelFrame.sample[c] = levelKnob * muteFactor * envelopeFrame.sample[c];
                        }

                        Frame outputFrame = panFrame(levelFrame, panChaos);
                        outMessage.soloCount += updateSolo(outMessage.soloAudio, outputFrame, args.sampleRate);
                        for (int c = 0; c < outMessage.wetAudio.nchannels; ++c)
                            outMessage.wetAudio.sample[c] += outputFrame.sample[c];
                    }

                    writeFrame(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, sendFrame, inMessage.polyphonic);
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, envelopeFrame.nchannels, envelopeFrame.sample);

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
                    addSapphireFlatControlGroup("level", LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT);
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
                        filterModule->updateMuteSoloControls();
                    }
                }

                void updateModeButton()
                {
                    if (filterModule)
                    {
                        const bool isBandpass = filterModule->modeFader.atFront();

                        filterModule->paramQuantities.at(MODE_BUTTON_PARAM)->name =
                            std::string("Mode: ") + (isBandpass ? "BANDPASS" : "NOTCH");
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

                void drawMuteIndicator(NVGcontext* vg)
                {
                    NVGcolor color = nvgRGBA(0x20, 0x20, 0x20, 0x40);
                    nvgBeginPath(vg);
                    nvgRect(vg, 0, 0, box.size.x, box.size.y);
                    nvgFillColor(vg, color);
                    nvgFill(vg);
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
                    for (unsigned c = 0; c < nchannels; ++c)
                        graphSpectrum(args, c);
            }
        }

        //----------------------------------------------------------------------------

        namespace EOutput
        {
            constexpr unsigned nChaoticSignals = 2;
            using fountain_t = ChaosFountain<nChaoticSignals>;
            using batch_t = ChaosBatch<nChaoticSignals>;

            enum ParamId
            {
                GLOBAL_MIX_PARAM,
                GLOBAL_MIX_ATTEN,
                GLOBAL_LEVEL_PARAM,
                GLOBAL_LEVEL_ATTEN,
                PARAMS_LEN
            };

            enum InputId
            {
                GLOBAL_MIX_CV_INPUT,
                GLOBAL_LEVEL_CV_INPUT,
                INPUTS_LEN
            };

            enum OutputId
            {
                AUDIO_LEFT_OUTPUT,
                AUDIO_RIGHT_OUTPUT,
                OUTPUTS_LEN
            };

            enum LightId
            {
                LIGHTS_LEN
            };

            struct OutputModule : EmpathModule
            {
                Crossfader firstSoloFader;      // crossfades the treansition between muting everyone else or not
                fountain_t fountain;

                explicit OutputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configStereoOutputs(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, "audio");
                    configControlGroup("Output mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT, 0, 1, 1, "%", 0, 100);
                    configControlGroup("Output level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                }

                void OutputModule_initialize()
                {
                    firstSoloFader.snapToFront();
                }

                void onReset(const ResetEvent& e) override
                {
                    EmpathModule::onReset(e);
                    OutputModule_initialize();
                    if (fountain.getSeed())
                        fountain.reset();
                }

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    json_object_set_new(root, "chaosFountainSeed", json_integer(fountain.getSeed()));
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    if (json_t* jseed = json_object_get(root, "chaosFountainSeed"); json_is_integer(jseed))
                        fountain.reset(json_integer_value(jseed));
                }


                void process(const ProcessArgs& args) override
                {
                    BackwardMessage backMessage;
                    const ForwardMessage message = receiveMessageOrDefault();
                    chainIndex = message.chainIndex;
                    backMessage.soloCount = message.soloCount;

                    includeNeonModeMenuItem = !message.valid;
                    if (message.valid)
                        neonMode = message.neonMode;

                    firstSoloFader.setTarget(message.soloCount > 0);
                    float solo = firstSoloFader.process(args.sampleRate, 0, 1);

                    const batch_t batch = fountain.process(
                        args.sampleRate,
                        message.chaosSpeedKnob,
                        message.chaosLevelKnob
                    );

                    Frame audio = outputAudioFrame(
                        message.dryAudio,
                        message.wetAudio,
                        message.soloAudio,
                        solo,
                        batch
                    );
                    writeFrame(AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT, audio, message.polyphonic);

                    sendBackwardMessage(backMessage);
                }

                Frame outputAudioFrame(
                    const Frame& dryAudio,
                    const Frame& wetAudio,
                    const Frame& soloAudio,
                    float soloFactor,
                    const batch_t& chaosBatch)
                {
                    const float mixChaos   = chaosBatch.signal.at(0);
                    const float levelChaos = chaosBatch.signal.at(1);

                    constexpr float gainSensitivity = 1.0 / 5.0;    // one knob unit per 5V change in CV
                    float cvMix = 0;
                    float cvLevel = 0;
                    const int nc = dryAudio.nchannels;
                    Frame result;
                    result.nchannels = nc;
                    for (int c = 0; c < nc; ++c)
                    {
                        nextVoltageOrChaosSignal(cvMix, GLOBAL_MIX_CV_INPUT, c, mixChaos);
                        nextVoltageOrChaosSignal(cvLevel, GLOBAL_LEVEL_CV_INPUT, c, levelChaos);
                        float level = Cube(cvGetVoltPerOctave(GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, cvLevel * gainSensitivity, 0, 2));
                        float mix = filter_t::filter_t::MixFactor(cvGetControlValue(GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, cvMix, 0, 1));
                        float wet = LinearMix(soloFactor, wetAudio.sample[c], soloAudio.sample[c]);
                        result.sample[c] = level * LinearMix(mix, dryAudio.sample[c], wet);
                    }
                    return result;
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
                    addSapphireControlGroup("global_level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT);
                }

                bool isConnectedOnLeft() const override
                {
                    return module && IsFilterSender(module->leftExpander.module);
                }

                bool isConnectedOnRight() const override
                {
                    return false;
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

        void EmpathWidget::addExpanderInsertButton(int paramId)
        {
            auto button = createParamCentered<InsertButton>(Vec{}, module, paramId);
            button->empathWidget = this;
            addSapphireParam(button, "insert_button");
        }
    }
}

Model* modelSapphireEmpathInput = createSapphireModel<Sapphire::Empath::EInput::InputModule, Sapphire::Empath::EInput::InputWidget>(
    "Empath",
    Sapphire::ExpanderRole::Empath
);

Model* modelSapphireEmpathFilter = createSapphireModel<Sapphire::Empath::Filter::FilterModule, Sapphire::Empath::Filter::FilterWidget>(
    "EmpathFilter",
    Sapphire::ExpanderRole::Empath
);

Model* modelSapphireEmpathOutput = createSapphireModel<Sapphire::Empath::EOutput::OutputModule, Sapphire::Empath::EOutput::OutputWidget>(
    "EmpathOutput",
    Sapphire::ExpanderRole::Empath
);
