#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Empath
    {
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
            Frame rawAudio;
        };

        struct BackwardMessage
        {
            bool valid = false;
            int soloCount = 0;  // how many filters are soloing right now
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


        struct EmpathModule : SapphireModule
        {
            int chainIndex = -1;
            float pendingMoveX{};
            int pendingMoveStepCount{};
            ForwardMessage forwardMessageBuffer[2];
            BackwardMessage backwardMessageBuffer[2];

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
                            // Mono input, so split the signal across both stereo output channels.
                            frame.sample[0] /= 2;
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
        };


        struct EmpathWidget : SapphireWidget
        {
            explicit EmpathWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
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
            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                AUDIO_LEFT_INPUT,
                AUDIO_RIGHT_INPUT,
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

            struct InputModule : EmpathModule
            {
                bool autoCreateExpanders = true;
                PortLabelMode inputLabels = PortLabelMode::Stereo;

                explicit InputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    chainIndex = 0;
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configStereoInputs(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                }

                json_t* dataToJson() override
                {
                    json_t* root = EmpathModule::dataToJson();
                    jsonSetBool(root, "autoCreateExpanders", autoCreateExpanders);
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    EmpathModule::dataFromJson(root);
                    jsonLoadBool(root, "autoCreateExpanders", autoCreateExpanders);
                }

                bool polyphonicMode()
                {
                    // FIXFIXFIX - reinstate when button exists
                    //return getParamQuantity(INPUT_MODE_BUTTON_PARAM)->getValue() > 0.5f;
                    return false;
                }

                void process(const ProcessArgs& args) override
                {
                    ForwardMessage outMessage;
                    outMessage.chainIndex = 1;
                    outMessage.neonMode = neonMode;
                    outMessage.polyphonic = polyphonicMode();
                    outMessage.rawAudio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, outMessage.polyphonic, inputLabels);
                    sendMessage(outMessage);
                }
            };

            struct InputWidget : EmpathWidget
            {
                InputModule* inputModule{};
                int expanderCountdown = 8;

                explicit InputWidget(InputModule* module)
                    : EmpathWidget("empath_input", asset::plugin(pluginInstance, "res/empath_input.svg"))
                    , inputModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(INSERT_BUTTON_PARAM);
                    addStereoInputPorts(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                }

                bool isConnectedOnLeft() const override
                {
                    return false;
                }

                bool isConnectedOnRight() const override
                {
                    return module && IsFilterReceiver(module->rightExpander.module);
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
                    }
                }
            };
        };

        //----------------------------------------------------------------------------

        namespace Filter
        {
            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                REMOVE_BUTTON_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
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

            struct FilterModule : EmpathModule
            {
                explicit FilterModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }

                void process(const ProcessArgs& args) override
                {
                    const ForwardMessage inMessage = receiveMessageOrDefault();
                    ForwardMessage outMessage = inMessage;

                    chainIndex = inMessage.chainIndex;

                    if (inMessage.chainIndex > 0)
                        outMessage.chainIndex = 1 + inMessage.chainIndex;

                    includeNeonModeMenuItem = !inMessage.valid;
                    if (inMessage.valid)
                        neonMode = inMessage.neonMode;

                    sendMessage(outMessage);
                }
            };

            struct FilterWidget : EmpathWidget
            {
                FilterModule* filterModule{};
                const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");

                explicit FilterWidget(FilterModule* module)
                    : EmpathWidget("empath_filter", asset::plugin(pluginInstance, "res/empath_filter.svg"))
                    , filterModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(INSERT_BUTTON_PARAM);
                    addExpanderRemoveButton(REMOVE_BUTTON_PARAM);
                }

                void addExpanderRemoveButton(int paramId)
                {
                    auto button = createParamCentered<RemoveButton>(Vec{}, filterModule, paramId);
                    button->empathWidget = this;
                    addSapphireParam(button, "remove_button");
                }

                bool isConnectedOnLeft() const override
                {
                    return module && IsFilterSender(module->leftExpander.module);
                }

                bool isConnectedOnRight() const override
                {
                    return module && IsFilterReceiver(module->rightExpander.module);
                }

                void draw(const DrawArgs& args) override
                {
                    EmpathWidget::draw(args);
                    if (filterModule)
                    {
                        if (!filterModule->neonMode)
                            drawChainIndex(args.vg, filterModule->chainIndex, nvgRGB(0x66, 0x06, 0x5c));
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
        }

        //----------------------------------------------------------------------------

        namespace EOutput
        {
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
                PortLabelMode outputLabels = PortLabelMode::Stereo;

                explicit OutputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                    configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                    configControlGroup("Output mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT, 0, 1, 0.5, "%", 0, 100);
                    configControlGroup("Output level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                }

                void process(const ProcessArgs& args) override
                {
                    // FIXFIXFIX: send backward message.

                    const ForwardMessage message = receiveMessageOrDefault();
                    chainIndex = message.chainIndex;

                    includeNeonModeMenuItem = !message.valid;
                    if (message.valid)
                        neonMode = message.neonMode;

                    Output& audioLeftOutput  = outputs.at(AUDIO_LEFT_OUTPUT);
                    Output& audioRightOutput = outputs.at(AUDIO_RIGHT_OUTPUT);
                    Frame audio = message.rawAudio;     // FIXFIXFIX - this is just to test the audio path
                    if (!message.polyphonic && audio.nchannels == 2)
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

            struct OutputWidget : EmpathWidget
            {
                OutputModule* outputModule{};

                explicit OutputWidget(OutputModule* module)
                    : EmpathWidget("empath_output", asset::plugin(pluginInstance, "res/empath_output.svg"))
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

                void draw(const DrawArgs& args) override
                {
                    EmpathWidget::draw(args);
                    PortLabelMode label = outputModule ? outputModule->outputLabels : PortLabelMode::Stereo;
                    ComponentLocation L = FindComponent(modcode, "output_label_left");
                    ComponentLocation R = FindComponent(modcode, "output_label_right");
                    drawAudioPortLabels(args.vg, label, L.cx, L.cy, R.cx, R.cy);
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
