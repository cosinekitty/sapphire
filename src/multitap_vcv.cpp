#include "sapphire_multitap.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        using insert_button_base_t = VCVLightBezel<WhiteLight>;

        inline bool IsModelType(const Module* module, const Model* model)
        {
            return module && model && module->model == model;
        }

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

        struct LoopWidget;

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

                message.nchannels = std::clamp(nchannels, 0, PORT_MAX_CHANNELS);
                for (int c = 0; c < message.nchannels; ++c)
                    message.audio[c] = audio[c];

                rightExpander.requestMessageFlip();
            }

            void sendMessage(const Message& message)
            {
                return sendMessage(message.nchannels, message.audio);
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
                {}
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

                void process(const ProcessArgs& args) override
                {
                    Input& audioLeftInput = inputs.at(AUDIO_LEFT_INPUT);
                    Input& audioRightInput = inputs.at(AUDIO_RIGHT_INPUT);

                    float frame[2];
                    frame[0] = audioLeftInput.getVoltageSum();
                    frame[1] = audioRightInput.getVoltageSum();
                    sendMessage(2, frame);
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

                void process(const ProcessArgs& args) override
                {
                    Message outMessage;     // null message with 0 audio channels

                    const Message* inMessage = receiveMessage();
                    if (inMessage != nullptr)
                    {
                        // FIXFIXFIX: for now we just copy the audio over. later we feed into a delay loop!
                        outMessage = *inMessage;
                    }

                    sendMessage(outMessage);
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
                        if (message->nchannels > 0)
                            left = message->audio[0];

                        if (message->nchannels > 1)
                            right = message->audio[1];
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
