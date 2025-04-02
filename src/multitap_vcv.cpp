#include "sapphire_multitap.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct LoopWidget;

        using insert_button_base_t = app::SvgSwitch;
        struct InsertButton : insert_button_base_t
        {
            LoopWidget* loopWidget{};

            explicit InsertButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/extender_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };

        struct MultiTapModule : SapphireModule
        {
            Message messageBuffer[2];
            int chainIndex = -1;

            explicit MultiTapModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
            {
                rightExpander.producerMessage = &messageBuffer[0];
                rightExpander.consumerMessage = &messageBuffer[1];
            }

            Message& rightMessageBuffer()
            {
                assert(rightExpander.producerMessage == &messageBuffer[0] || rightExpander.producerMessage == &messageBuffer[1]);
                return *static_cast<Message*>(rightExpander.producerMessage);
            }

            void sendMessage(int nchannels, const float* audio)
            {
                Message message;

                message.audio.nchannels = std::clamp(nchannels, 0, PORT_MAX_CHANNELS);
                for (int c = 0; c < message.audio.nchannels; ++c)
                    message.audio.sample[c] = audio[c];

                sendMessage(message);
            }

            void sendMessage(const Message& inMessage)
            {
                Message& outMessage = rightMessageBuffer();
                outMessage = inMessage;
                outMessage.chainIndex = (chainIndex < 0) ? -1 : (1 + chainIndex);
                rightExpander.requestMessageFlip();
            }

            const Message* receiveMessage()
            {
                // InLoop modules do not receive expander messages.
                if (!IsInLoop(this))
                {
                    // Look to the left for an InLoop or Loop module.
                    const Module* left = leftExpander.module;
                    if (IsInLoop(left) || IsLoop(left))
                        return static_cast<const Message*>(left->rightExpander.consumerMessage);
                }
                return nullptr;
            }
        };

        struct LoopModule : MultiTapModule
        {
            explicit LoopModule(std::size_t nParams, std::size_t nOutputPorts)
                : MultiTapModule(nParams, nOutputPorts)
            {
            }

            Result calculate(float sampleRateHz, const Message& inMessage, const InputState& input) const
            {
                // Calculating and return a Result as a function of Message and InputState.
                // The caller performs actual mutations such as forwarding the message
                // and applying updates to output ports.

                Result result;
                OutputState output;

                // FIXFIXFIX: do some actual processing here!!!
                // For now, mix input audio with bus-message audio.
                result.message.audio = inMessage.audio + input.audio;
                result.output = output;

                return result;
            }

            virtual InputState getInputs() = 0;
            virtual void setOutputs(const OutputState& output) = 0;

            void process(const ProcessArgs& args) override
            {
                Message inMessage;
                const Message* ptr = receiveMessage();
                if (ptr != nullptr)
                    inMessage = *ptr;

                if (IsLoop(this))
                    chainIndex = inMessage.chainIndex;

                InputState input = getInputs();
                Result result = calculate(args.sampleRate, inMessage, input);
                setOutputs(result.output);
                sendMessage(result.message);
            }

            void configTimeControls(int paramId, int attenId, int cvInputId)
            {
                const float L1 = std::log2(0.025);
                const float L2 = std::log2(10.0);
                configParam(paramId, L1, L2, -1, "Delay time", " sec", 2, 1);
                configParam(attenId, -1, +1, 0, "Delay time attenuverter", "%", 0, 100);
                configInput(cvInputId, "Delay time CV");
            }

            void configFeedbackControls(int paramId, int attenId, int cvInputId)
            {
                configParam(paramId, 0, 1, 0, "Feedback amount", "%", 0, 100);
                configParam(attenId, -1, +1, 0, "Feedback amount attenuverter", "%", 0, 100);
                configInput(cvInputId, "Feedback amount CV");
            }

            void configPanControls(int paramId, int attenId, int cvInputId)
            {
                configParam(paramId, -1, +1, 0, "Panning", "%", 0, 100);
                configParam(attenId, -1, +1, 0, "Panning attenuverter", "%", 0, 100);
                configInput(cvInputId, "Panning CV");
            }

            void configMixControls(int paramId, int attenId, int cvInputId)
            {
                configParam(paramId, 0, 1, 1, "Mix", "%", 0, 100);
                configParam(attenId, -1, +1, 0, "Mix attenuverter", "%", 0, 100);
                configInput(cvInputId, "Mix CV");
            }

            void configGainControls(int paramId, int attenId, int cvInputId)
            {
                configParam(paramId, 0, 1, 1, "Gain", " dB", -10, 20);
                configParam(attenId, -1, +1, 0, "Gain attenuverter", "%", 0, 100);
                configInput(cvInputId, "Gain CV");
            }
        };


        struct LoopWidget : SapphireWidget
        {
            const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");

            explicit LoopWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            void addExpanderInsertButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createParamCentered<InsertButton>(Vec{}, loopModule, paramId);
                button->loopWidget = this;
                addSapphireParam(button, "insert_button");
            }

            void insertExpander()
            {
                if (module == nullptr)
                    return;

                // We either insert a "Loop" module or an "OutLoop" module, depending on the situation.
                // If the module to the right is a Loop or an OutLoop, insert another Loop.
                // Otherwise, assume we are at the end of a chain that is not terminated by
                // an OutLoop, so insert an OutLoop.

                Module* right = module->rightExpander.module;

                Model* model =
                    (IsLoop(right) || IsOutLoop(right))
                    ? modelSapphireLoop
                    : modelSapphireOutLoop;

                // Erase any obsolete chain indices already in the remaining modules.
                // This prevents them briefly flashing on the screen before being replaced.
                for (Module* node = right; IsLoop(node) || IsOutLoop(node); node = node->rightExpander.module)
                {
                    auto lmod = dynamic_cast<MultiTapModule*>(node);
                    lmod->chainIndex = -1;
                }

                // Create the expander module.
                AddExpander(model, this, ExpanderDirection::Right);
            }

            virtual bool isConnectedOnLeft() const = 0;

            bool isConnectedOnRight() const
            {
                return module && (IsLoop(module->rightExpander.module) || IsOutLoop(module->rightExpander.module));
            }

            void step() override
            {
                SapphireWidget::step();
                SapphireModule* smod = getSapphireModule();
                if (smod != nullptr)
                {
                    smod->hideLeftBorder  = isConnectedOnLeft();
                    smod->hideRightBorder = isConnectedOnRight();
                }
            }

            void drawChainIndex(NVGcontext* vg, int chainIndex)
            {
                if (module == nullptr)
                    return;

                if (chainIndex < 1)
                    return;

                if (IsInLoop(module))
                {
                    const Module* right = module->rightExpander.module;
                    if (!IsLoop(right) && !IsOutLoop(right))
                        return;
                }

                std::shared_ptr<Font> font = APP->window->loadFont(chainFontPath);
                if (font)
                {
                    const float yCenter_mm = 11.0;
                    const NVGcolor textColor = nvgRGB(0x66, 0x06, 0x5c);

                    float bounds[4]{};  // [xmin, ymin, xmax, ymax]
                    char text[20];
                    snprintf(text, sizeof(text), "%d", chainIndex);

                    nvgFontSize(vg, 18);
                    nvgFontFaceId(vg, font->handle);
                    nvgFillColor(vg, textColor);

                    nvgTextBounds(vg, 0, 0, text, nullptr, bounds);
                    float width  = bounds[2] - bounds[0];
                    float height = bounds[3] - bounds[1];
                    float x1 = ((box.size.x - width) / 2);
                    float y1 = mm2px(yCenter_mm) - height/2;

                    if (chainIndex == 1)
                        x1 += mm2px(INLOOP_XSHIFT_MM);     // shift right on the InLoop module, because it is wider

                    nvgText(vg, x1, y1, text, nullptr);
                }
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                SapphireWidget::drawLayer(args, layer);
                if (layer == 1)
                {
                    const LoopModule *lmod = dynamic_cast<const LoopModule*>(module);
                    if (lmod != nullptr)
                    {
                        drawChainIndex(args.vg, lmod->chainIndex);
                    }
                }
            }
        };


        void InsertButton::onButton(const event::Button& e)
        {
            insert_button_base_t::onButton(e);
            if (loopWidget != nullptr)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    loopWidget->insertExpander();
            }
        }


        namespace InLoop
        {
            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                TIME_PARAM,
                TIME_ATTEN,
                FEEDBACK_PARAM,
                FEEDBACK_ATTEN,
                PAN_PARAM,
                PAN_ATTEN,
                MIX_PARAM,
                MIX_ATTEN,
                GAIN_PARAM,
                GAIN_ATTEN,
                PARAMS_LEN
            };

            enum InputId
            {
                AUDIO_LEFT_INPUT,
                AUDIO_RIGHT_INPUT,
                TIME_CV_INPUT,
                FEEDBACK_CV_INPUT,
                PAN_CV_INPUT,
                MIX_CV_INPUT,
                GAIN_CV_INPUT,
                INPUTS_LEN
            };

            enum OutputId
            {
                OUTPUTS_LEN
            };

            enum LightId
            {
                INSERT_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct Mod : LoopModule
            {
                Mod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    chainIndex = 1;
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Add tap");
                    configInput(AUDIO_LEFT_INPUT,  "Left audio");
                    configInput(AUDIO_RIGHT_INPUT, "Right audio");
                    configTimeControls(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    configFeedbackControls(FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    configPanControls(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    configMixControls(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    initialize();
                }

                void initialize()
                {
                }

                void onReset(const ResetEvent& e) override
                {
                    Module::onReset(e);
                    initialize();
                }

                InputState getInputs() override
                {
                    Input& audioLeftInput = inputs.at(AUDIO_LEFT_INPUT);
                    Input& audioRightInput = inputs.at(AUDIO_RIGHT_INPUT);

                    InputState state;
                    state.audio.nchannels = 2;
                    state.audio.sample[0] = audioLeftInput.getVoltageSum();
                    state.audio.sample[1] = audioRightInput.getVoltageSum();
                    return state;
                }

                void setOutputs(const OutputState& output) override
                {
                }
            };

            struct Wid : LoopWidget
            {
                Mod* inLoopModule{};

                explicit Wid(Mod* module)
                    : LoopWidget("inloop", asset::plugin(pluginInstance, "res/inloop.svg"))
                    , inLoopModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addSapphireInput(AUDIO_LEFT_INPUT,  "audio_left_input");
                    addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");
                    addSapphireFlatControlGroup("time", TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    addSapphireFlatControlGroup("feedback", FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("mix", MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                }

                bool isConnectedOnLeft() const override
                {
                    return false;
                }
            };
        }

        namespace Loop
        {
            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                TIME_PARAM,
                TIME_ATTEN,
                FEEDBACK_PARAM,
                FEEDBACK_ATTEN,
                PAN_PARAM,
                PAN_ATTEN,
                MIX_PARAM,
                MIX_ATTEN,
                GAIN_PARAM,
                GAIN_ATTEN,
                PARAMS_LEN
            };

            enum InputId
            {
                TIME_CV_INPUT,
                FEEDBACK_CV_INPUT,
                PAN_CV_INPUT,
                MIX_CV_INPUT,
                GAIN_CV_INPUT,
                INPUTS_LEN
            };

            enum OutputId
            {
                OUTPUTS_LEN
            };

            enum LightId
            {
                INSERT_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct Mod : LoopModule
            {
                Mod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Add tap");
                    configTimeControls(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    configFeedbackControls(FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    configPanControls(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    configMixControls(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    initialize();
                }

                void initialize()
                {
                }

                void onReset(const ResetEvent& e) override
                {
                    Module::onReset(e);
                    initialize();
                }

                InputState getInputs() override
                {
                    InputState state;
                    return state;
                }

                void setOutputs(const OutputState& output) override
                {
                }
            };

            struct Wid : LoopWidget
            {
                Mod* loopModule{};

                explicit Wid(Mod* module)
                    : LoopWidget("loop", asset::plugin(pluginInstance, "res/loop.svg"))
                    , loopModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addSapphireFlatControlGroup("time", TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    addSapphireFlatControlGroup("feedback", FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("mix", MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                }

                bool isConnectedOnLeft() const override
                {
                    return module && (IsInLoop(module->leftExpander.module) || IsLoop(module->leftExpander.module));
                }
            };
        }

        namespace OutLoop
        {
            enum ParamId
            {
                PARAMS_LEN
            };

            enum InputId
            {
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

            struct Mod : MultiTapModule
            {
                Mod()
                    : MultiTapModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                    configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                    initialize();
                }

                void initialize()
                {
                }

                void onReset(const ResetEvent& e) override
                {
                    Module::onReset(e);
                    initialize();
                }

                void process(const ProcessArgs& args) override
                {
                    float left = 0;
                    float right = 0;

                    const Message* message = receiveMessage();
                    if (message != nullptr)
                    {
                        if (message->audio.nchannels > 0)
                            left = message->audio.sample[0];

                        if (message->audio.nchannels > 1)
                            right = message->audio.sample[1];

                        chainIndex = message->chainIndex;
                    }
                    else
                    {
                        chainIndex = -1;
                    }

                    Output& audioLeftOutput  = outputs.at(AUDIO_LEFT_OUTPUT);
                    Output& audioRightOutput = outputs.at(AUDIO_RIGHT_OUTPUT);

                    audioLeftOutput.setChannels(1);
                    audioLeftOutput.setVoltage(left, 0);

                    audioRightOutput.setChannels(1);
                    audioRightOutput.setVoltage(right, 0);
                }
            };

            struct Wid : SapphireWidget
            {
                Mod* outLoopModule{};

                explicit Wid(Mod* module)
                    : SapphireWidget("outloop", asset::plugin(pluginInstance, "res/outloop.svg"))
                    , outLoopModule(module)
                {
                    setModule(module);
                    addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                    addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                }

                bool isConnectedOnLeft() const
                {
                    return module && (IsInLoop(module->leftExpander.module) || IsLoop(module->leftExpander.module));
                }

                void step() override
                {
                    SapphireWidget::step();
                    SapphireModule* smod = getSapphireModule();
                    if (smod != nullptr)
                        smod->hideLeftBorder = isConnectedOnLeft();
                }
            };
        }
    }
}


Model* modelSapphireInLoop = createSapphireModel<Sapphire::MultiTap::InLoop::Mod, Sapphire::MultiTap::InLoop::Wid>(
    "InLoop",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireLoop = createSapphireModel<Sapphire::MultiTap::Loop::Mod, Sapphire::MultiTap::Loop::Wid>(
    "Loop",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireOutLoop = createSapphireModel<Sapphire::MultiTap::OutLoop::Mod, Sapphire::MultiTap::OutLoop::Wid>(
    "OutLoop",
    Sapphire::ExpanderRole::MultiTap
);
