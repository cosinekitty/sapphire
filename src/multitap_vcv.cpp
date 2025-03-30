#include "sapphire_multitap.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct LoopWidget;

        using insert_button_base_t = VCVLightBezel<WhiteLight>;
        struct InsertButton : insert_button_base_t
        {
            LoopWidget* loopWidget{};

            void onButton(const event::Button& e) override;
        };

        struct MultiTapModule : SapphireModule
        {
            Message messageBuffer[2];

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
                Message& message = rightMessageBuffer();

                message.audio.nchannels = std::clamp(nchannels, 0, PORT_MAX_CHANNELS);
                for (int c = 0; c < message.audio.nchannels; ++c)
                    message.audio.sample[c] = audio[c];

                rightExpander.requestMessageFlip();
            }

            void sendMessage(const Message& message)
            {
                return sendMessage(message.audio.nchannels, message.audio.sample);
            }

            const Message* receiveMessage()
            {
                // Look to the left for an InLoop or Loop module.
                const Module* left = leftExpander.module;
                if (IsInLoop(left) || IsLoop(left))
                    return static_cast<const Message*>(left->rightExpander.consumerMessage);

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
                // As an experiment, I take a completely functional-programming
                // approach here, calculating a Result as a function of
                // Message and InputState.
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

                InputState input = getInputs();
                Result result = calculate(args.sampleRate, inMessage, input);
                setOutputs(result.output);
                sendMessage(result.message);
            }
        };


        struct LoopWidget : SapphireWidget
        {
            explicit LoopWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            void addExpanderInsertButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createLightParamCentered<InsertButton>(Vec{}, loopModule, paramId, lightId);
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

                const Module* right = module->rightExpander.module;
                Model* model =
                    (IsLoop(right) || IsOutLoop(right))
                    ? modelSapphireLoop
                    : modelSapphireOutLoop;

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
                INSERT_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct Mod : LoopModule
            {
                Mod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Insert loop expander");
                    configInput(AUDIO_LEFT_INPUT,  "Left audio");
                    configInput(AUDIO_RIGHT_INPUT, "Right audio");
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
                INSERT_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct Mod : LoopModule
            {
                Mod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Insert loop expander");
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
