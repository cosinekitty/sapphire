#include "sapphire_multitap.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct LoopWidget;

        enum class
        TimeMode
        {
            Seconds,
            ClockSync,
            LEN
        };

        using time_knob_base_t = RoundSmallBlackKnob;
        struct TimeKnob : time_knob_base_t
        {
            TimeMode* mode = nullptr;     // should point into the module, when the module exists

            void drawLayer(const DrawArgs& args, int layer) override
            {
                time_knob_base_t::drawLayer(args, layer);

                if (layer != 1)
                    return;

                if (mode == nullptr)
                    return;

                switch (*mode)
                {
                case TimeMode::Seconds:
                default:
                    // Do not draw anything on the knob
                    break;

                case TimeMode::ClockSync:
                    drawClockSyncSymbol(args.vg);
                    break;
                }
            }

            void drawClockSyncSymbol(NVGcontext* vg)
            {
                float dx = 5.0;
                float x2 = box.size.x / 2;
                float x1 = x2 - dx;
                float x3 = x2 + dx;

                float dy = 4.0;
                float ym = box.size.y / 2;
                float y1 = ym - dy;
                float y2 = ym + dy;

                nvgBeginPath(vg);
                nvgStrokeColor(vg, SCHEME_GREEN);
                nvgMoveTo(vg, x1, y1);
                nvgLineTo(vg, x1, y2);
                nvgLineTo(vg, x3, y2);
                nvgLineTo(vg, x3, y1);
                nvgLineTo(vg, x1, y1);
                nvgStrokeWidth(vg, 0.75);
                nvgStroke(vg);
            }

            void appendContextMenu(Menu* menu) override
            {
                if (mode == nullptr)
                    return;

                menu->addChild(new MenuSeparator);
                menu->addChild(createIndexSubmenuItem(
                    "Time mode",
                    {
                        "Seconds",
                        "Clock sync"
                    },
                    [=](){ return static_cast<size_t>(*mode); },
                    [=](size_t index){ *mode = static_cast<TimeMode>(index); }
                ));
            }
        };

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
                MultiTap_initialize();
            }

            void MultiTap_initialize()
            {
            }

            virtual void initialize()
            {
                MultiTap_initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            Message& rightMessageBuffer()
            {
                assert(rightExpander.producerMessage == &messageBuffer[0] || rightExpander.producerMessage == &messageBuffer[1]);
                return *static_cast<Message*>(rightExpander.producerMessage);
            }

            void sendMessage(const Message& message)
            {
                Message& destination = rightMessageBuffer();
                destination = message;
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

            Frame readFrame(int leftInputId, int rightInputId)
            {
                Frame frame;

                Input& inLeft  = inputs.at(leftInputId);
                Input& inRight = inputs.at(rightInputId);

                frame.nchannels = 2;
                frame.sample[0] = inLeft.getVoltageSum();
                frame.sample[1] = inRight.getVoltageSum();

                return frame;
            }
        };

        struct LoopModule : MultiTapModule
        {
            bool frozen = false;
            bool reversed = false;
            bool clearBufferRequested = false;
            TimeMode timeMode = TimeMode::Seconds;
            GateTriggerReceiver reverseReceiver;

            explicit LoopModule(std::size_t nParams, std::size_t nOutputPorts)
                : MultiTapModule(nParams, nOutputPorts)
            {
                Loop_initialize();
            }

            void Loop_initialize()
            {
                frozen = false;
                reversed = false;
                reverseReceiver.initialize();
                clearBuffer();
            }

            void initialize() override
            {
                MultiTapModule::initialize();
                Loop_initialize();
            }

            bool isFrozen() const
            {
                return frozen;
            }

            bool isReversed() const
            {
                return reversed;
            }

            void clearBuffer()
            {
                clearBufferRequested = false;
            }

            bool updateToggleState(GateTriggerReceiver& receiver, int buttonParamId, int inputId, int lightId)
            {
                bool flag = updateToggleGroup(receiver, inputId, buttonParamId);
                setLightBrightness(lightId, flag);
                return flag;
            }

            json_t* dataToJson() override
            {
                json_t* root = MultiTapModule::dataToJson();
                json_object_set_new(root, "timeMode", json_integer(static_cast<int>(timeMode)));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                MultiTapModule::dataFromJson(root);
                json_t* jsTimeMode = json_object_get(root, "timeMode");
                if (json_is_integer(jsTimeMode))
                    timeMode = static_cast<TimeMode>(json_integer_value(jsTimeMode));
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
                configParam(paramId, 0, 1, 1, "Level", " dB", -10, 20);
                configParam(attenId, -1, +1, 0, "Level attenuverter", "%", 0, 100);
                configInput(cvInputId, "Level CV");
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
                    const float yCenter_mm = 10.0;
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

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);

                auto lmod = dynamic_cast<const LoopModule*>(module);
                if (lmod != nullptr)
                {
                    drawChainIndex(args.vg, lmod->chainIndex);
                }
            }

            void appendContextMenu(Menu *menu) override
            {
                SapphireWidget::appendContextMenu(menu);

                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod != nullptr)
                {
                    menu->addChild(new MenuSeparator);
                    menu->addChild(lmod->createToggleAllSensitivityMenuItem());
                }
            }

            TimeKnob* addTimeControlGroup(int paramId, int attenId, int cvInputId)
            {
                TimeKnob* tk = addSapphireFlatControlGroup<TimeKnob>("time", paramId, attenId, cvInputId);
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod != nullptr)
                    tk->mode = &(lmod->timeMode);
                return tk;
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
                REVERSE_BUTTON_PARAM,
                FREEZE_BUTTON_PARAM,
                CLEAR_BUTTON_PARAM,
                ENV_GAIN_PARAM,
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
                RETURN_LEFT_INPUT,
                RETURN_RIGHT_INPUT,
                REVERSE_INPUT,
                FREEZE_INPUT,
                CLEAR_INPUT,
                CLOCK_INPUT,
                INPUTS_LEN
            };

            enum OutputId
            {
                SEND_LEFT_OUTPUT,
                SEND_RIGHT_OUTPUT,
                ENV_OUTPUT,
                OUTPUTS_LEN
            };

            enum LightId
            {
                INSERT_BUTTON_LIGHT,
                REVERSE_BUTTON_LIGHT,
                FREEZE_BUTTON_LIGHT,
                CLEAR_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct InMod : LoopModule
            {
                // Global controls
                GateTriggerReceiver freezeReceiver;
                AnimatedTriggerReceiver clearReceiver;

                InMod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    chainIndex = 1;
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configButton(INSERT_BUTTON_PARAM, "Add tap");
                    configStereoInputs(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                    configStereoOutputs(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    configOutput(ENV_OUTPUT, "Envelope follower");
                    configStereoInputs(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    configTimeControls(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    configFeedbackControls(FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    configPanControls(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    configMixControls(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    configToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, "Reverse", "Reverse");
                    configToggleGroup(FREEZE_INPUT, FREEZE_BUTTON_PARAM, "Freeze", "Freeze");
                    configToggleGroup(CLEAR_INPUT, CLEAR_BUTTON_PARAM, "Clear", "Clear");
                    configInput(CLOCK_INPUT, "Clock");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    InLoop_initialize();
                }

                void InLoop_initialize()
                {
                    freezeReceiver.initialize();
                    clearReceiver.initialize();
                }

                void initialize() override
                {
                    LoopModule::initialize();
                    InLoop_initialize();
                }

                void process(const ProcessArgs& args) override
                {
                    Message outMessage;
                    frozen = outMessage.frozen = updateFreezeState();
                    reversed = updateReverseState();
                    updateClearState(args.sampleRate);
                    outMessage.chainIndex = 2;
                    outMessage.originalAudio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);
                    outMessage.chainAudio = outMessage.originalAudio;
                    sendMessage(outMessage);
                }

                bool updateFreezeState()
                {
                    return updateToggleState(
                        freezeReceiver,
                        FREEZE_BUTTON_PARAM,
                        FREEZE_INPUT,
                        FREEZE_BUTTON_LIGHT
                    );
                }

                bool updateReverseState()
                {
                    return updateToggleState(
                        reverseReceiver,
                        REVERSE_BUTTON_PARAM,
                        REVERSE_INPUT,
                        REVERSE_BUTTON_LIGHT
                    );
                }

                bool updateClearState(float sampleRateHz)
                {
                    return updateTriggerGroup(
                        sampleRateHz,
                        clearReceiver,
                        CLEAR_INPUT,
                        CLEAR_BUTTON_PARAM,
                        CLEAR_BUTTON_LIGHT
                    );
                }
            };

            struct InWid : LoopWidget
            {
                InMod* inLoopModule{};

                explicit InWid(InMod* module)
                    : LoopWidget("inloop", asset::plugin(pluginInstance, "res/inloop.svg"))
                    , inLoopModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);

                    // Global controls/ports
                    addStereoInputPorts(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                    addSapphireControlGroup("feedback", FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    addFreezeToggleGroup();
                    addClearTriggerGroup();
                    addSapphireInput(CLOCK_INPUT, "clock_input");

                    // Per-tap controls/ports
                    addStereoOutputPorts(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    addStereoInputPorts(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    addTimeControlGroup(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);

                    addToggleGroup("reverse", REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, '\0', 0.0, SCHEME_PURPLE);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("mix", MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    addSapphireOutput(ENV_OUTPUT, "env_output");
                    addSmallKnob(ENV_GAIN_PARAM, "env_gain_knob");
                }

                SapphireCaptionButton* addFreezeToggleGroup()
                {
                    SapphireCaptionButton* freezeButton = addToggleGroup("freeze", FREEZE_INPUT, FREEZE_BUTTON_PARAM, FREEZE_BUTTON_LIGHT, '\0', 0.0, SCHEME_BLUE);
                    return freezeButton;
                }

                void addClearTriggerGroup()
                {
                    addToggleGroup(
                        "clear",
                        CLEAR_INPUT,
                        CLEAR_BUTTON_PARAM,
                        CLEAR_BUTTON_LIGHT,
                        '\0',
                        0.0,
                        SCHEME_GREEN,
                        true
                    );
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
                PAN_PARAM,
                PAN_ATTEN,
                MIX_PARAM,
                MIX_ATTEN,
                GAIN_PARAM,
                GAIN_ATTEN,
                REVERSE_BUTTON_PARAM,
                ENV_GAIN_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                TIME_CV_INPUT,
                PAN_CV_INPUT,
                MIX_CV_INPUT,
                GAIN_CV_INPUT,
                RETURN_LEFT_INPUT,
                RETURN_RIGHT_INPUT,
                REVERSE_INPUT,
                INPUTS_LEN
            };

            enum OutputId
            {
                SEND_LEFT_OUTPUT,
                SEND_RIGHT_OUTPUT,
                ENV_OUTPUT,
                OUTPUTS_LEN
            };

            enum LightId
            {
                INSERT_BUTTON_LIGHT,
                REVERSE_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            struct TapMod : LoopModule
            {
                TapMod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configStereoOutputs(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    configStereoInputs(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    configOutput(ENV_OUTPUT, "Envelope follower");
                    configButton(INSERT_BUTTON_PARAM, "Add tap");
                    configTimeControls(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    configPanControls(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    configMixControls(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    configToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, "Reverse", "Reverse");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    Tap_initialize();
                }

                void Tap_initialize()
                {
                }

                void initialize() override
                {
                    LoopModule::initialize();
                    Tap_initialize();
                }

                void process(const ProcessArgs& args) override
                {
                    Message inMessage;
                    const Message* ptr = receiveMessage();
                    if (ptr != nullptr)
                        inMessage = *ptr;

                    // Copy input to output by default, then patch whatever is different.
                    Message outMessage = inMessage;
                    chainIndex = inMessage.chainIndex;
                    frozen = inMessage.frozen;

                    reversed = updateReverseState();
                    outMessage.chainIndex = (chainIndex < 0) ? -1 : (1 + chainIndex);
                    sendMessage(outMessage);
                }

                bool updateReverseState()
                {
                    return updateToggleState(
                        reverseReceiver,
                        REVERSE_BUTTON_PARAM,
                        REVERSE_INPUT,
                        REVERSE_BUTTON_LIGHT
                    );
                }
            };

            struct TapWid : LoopWidget
            {
                TapMod* loopModule{};

                explicit TapWid(TapMod* module)
                    : LoopWidget("loop", asset::plugin(pluginInstance, "res/loop.svg"))
                    , loopModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addStereoOutputPorts(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    addStereoInputPorts(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    addTimeControlGroup(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("mix", MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    addToggleGroup("reverse", REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, '\0', 0.0, SCHEME_PURPLE);
                    addSapphireOutput(ENV_OUTPUT, "env_output");
                    addSmallKnob(ENV_GAIN_PARAM, "env_gain_knob");
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

            struct OutMod : MultiTapModule
            {
                OutMod()
                    : MultiTapModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                    configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                    configControlGroup("Output mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT, 0, 1, 1, "%", 0, 100);
                    configControlGroup("Output level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*4);
                    OutLoop_initialize();
                }

                void OutLoop_initialize()
                {
                }

                void initialize() override
                {
                    MultiTapModule::initialize();
                    OutLoop_initialize();
                }

                void process(const ProcessArgs& args) override
                {
                    float left = 0;
                    float right = 0;

                    const Message* message = receiveMessage();
                    if (message != nullptr)
                    {
                        if (message->chainAudio.nchannels > 0)
                            left = message->chainAudio.sample[0];

                        if (message->chainAudio.nchannels > 1)
                            right = message->chainAudio.sample[1];

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

            struct OutWid : SapphireWidget
            {
                OutMod* outLoopModule{};

                explicit OutWid(OutMod* module)
                    : SapphireWidget("outloop", asset::plugin(pluginInstance, "res/outloop.svg"))
                    , outLoopModule(module)
                {
                    setModule(module);
                    addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                    addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                    addSapphireControlGroup("global_mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT);
                    addSapphireControlGroup("global_level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT);
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


Model* modelSapphireInLoop = createSapphireModel<Sapphire::MultiTap::InLoop::InMod, Sapphire::MultiTap::InLoop::InWid>(
    "InLoop",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireLoop = createSapphireModel<Sapphire::MultiTap::Loop::TapMod, Sapphire::MultiTap::Loop::TapWid>(
    "Loop",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireOutLoop = createSapphireModel<Sapphire::MultiTap::OutLoop::OutMod, Sapphire::MultiTap::OutLoop::OutWid>(
    "OutLoop",
    Sapphire::ExpanderRole::MultiTap
);
