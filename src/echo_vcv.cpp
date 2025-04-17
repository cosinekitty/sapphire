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
        };

        using time_knob_base_t = RoundSmallBlackKnob;
        struct TimeKnob : time_knob_base_t
        {
            TimeMode* mode = nullptr;     // should point into the module, when the module exists
            bool* isClockConnected = nullptr;

            TimeMode getMode() const
            {
                return mode ? *mode : TimeMode::Seconds;
            }

            bool isClockCableConnected() const
            {
                return isClockConnected && *isClockConnected;
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                time_knob_base_t::drawLayer(args, layer);

                if (layer != 1)
                    return;

                switch (getMode())
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
                const bool connected = isClockCableConnected();

                float dx = 5.0;
                float x2 = box.size.x / 2;
                float x1 = x2 - dx;
                float x3 = x2 + dx;

                float dy = 4.0;
                float ym = box.size.y / 2;
                float y1 = ym - dy;
                float y2 = ym + dy;

                const NVGcolor inactiveColor = nvgRGB(0x90, 0x90, 0x90);

                nvgBeginPath(vg);
                nvgStrokeColor(vg, connected ? SCHEME_CYAN : inactiveColor);
                nvgMoveTo(vg, x1, y1);
                nvgLineTo(vg, x1, y2);
                nvgLineTo(vg, x3, y2);
                nvgLineTo(vg, x3, y1);
                nvgLineTo(vg, x1, y1);
                nvgStrokeWidth(vg, connected ? 1.5 : 1.0);
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

        struct TimeKnobParamQuantity : ParamQuantity
        {
            std::string getString() override;
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
            bool receivedMessageFromLeft = false;

            explicit MultiTapModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
            {
                rightExpander.producerMessage = &messageBuffer[0];
                rightExpander.consumerMessage = &messageBuffer[1];
                MultiTapModule_initialize();
            }

            void MultiTapModule_initialize()
            {
            }

            virtual void initialize()
            {
                MultiTapModule_initialize();
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
                if (IsEchoReceiver(this) && IsEchoSender(leftExpander.module))
                    return static_cast<const Message*>(leftExpander.module->rightExpander.consumerMessage);
                return nullptr;
            }

            Message receiveMessageOrDefault()
            {
                const Message* ptr = receiveMessage();
                receivedMessageFromLeft = (ptr != nullptr);
                return receivedMessageFromLeft ? *ptr : Message{};
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
            const float L1 = std::log2(TAPELOOP_MIN_DELAY_SECONDS);
            const float L2 = std::log2(TAPELOOP_MAX_DELAY_SECONDS);
            bool unhappy = false;
            bool frozen = false;
            bool reversed = false;
            bool clearBufferRequested = false;
            bool isClockConnected = false;
            TimeMode timeMode = TimeMode::Seconds;
            GateTriggerReceiver reverseReceiver;
            EnvelopeFollower env;
            ChannelInfo info[PORT_MAX_CHANNELS];
            PolyControls controls;

            explicit LoopModule(std::size_t nParams, std::size_t nOutputPorts)
                : MultiTapModule(nParams, nOutputPorts)
            {
                LoopModule_initialize();
            }

            void LoopModule_initialize()
            {
                frozen = false;
                reversed = false;
                reverseReceiver.initialize();
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    info[c].initialize();
                clearBufferRequested = true;
                unhappy = false;
            }

            void initialize() override
            {
                MultiTapModule::initialize();
                LoopModule_initialize();
            }

            bool isFrozen() const
            {
                return frozen;
            }

            bool isReversed() const
            {
                return reversed;
            }

            bool isActivelyClocked() const
            {
                // Actively clocked means both are true:
                // 1. The user enabled clocking on this tape loop.
                // 2. There is a cable connected to the CLOCK input port.
                return (timeMode == TimeMode::ClockSync) && isClockConnected;
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

            void updateEnvelope(int outputId, int envGainParamId, float sampleRateHz, const Frame& audio)
            {
                const int nc = VcvSafeChannelCount(audio.nchannels);
                float sum = 0;
                for (int c = 0; c < nc; ++c)
                    sum += audio.sample[c];

                float knob = params.at(envGainParamId).getValue();
                float v = FourthPower(knob) * env.update(sum, sampleRateHz);
                Output& envOutput = outputs.at(outputId);
                envOutput.setChannels(1);
                envOutput.setVoltage(v, 0);
            }

            ChannelInfo& getChannelInfo(int c)
            {
                return SafeArray(info, PORT_MAX_CHANNELS, c);
            }

            struct TapeLoopResult
            {
                Frame outAudio;
                Frame clockVoltage;
            };

            TapeLoopResult updateTapeLoops(
                const Frame& inAudio,
                float sampleRateHz,
                const Message& message)
            {
                const int nc = inAudio.safeChannelCount();
                const float two = 2;    // silly, but helps for portability to MAC/ARM

                Output& sendLeft   = outputs.at(controls.sendLeftOutputId);
                Output& sendRight  = outputs.at(controls.sendRightOutputId);
                Input& returnLeft  = inputs.at(controls.returnLeftInputId);
                Input& returnRight = inputs.at(controls.returnRightInputId);

                TapeLoopResult result;
                result.outAudio.nchannels = nc;
                result.clockVoltage.nchannels = nc;

                // First pass: read delay line outputs from calculated times into the past.
                Frame delayLineOutput;
                delayLineOutput.nchannels = nc;
                float cvDelayTime = 0;
                float vClock = 0;
                for (int c = 0; c < nc; ++c)
                {
                    ChannelInfo& q = getChannelInfo(c);

                    float clockSyncTime = -1;
                    if (controls.clockInputId < 0)
                        vClock = message.clockVoltage.at(c);
                    else
                        vClock = nextChannelInputVoltage(vClock, controls.clockInputId, c);
                    result.clockVoltage.at(c) = vClock;
                    if (q.clockReceiver.updateTrigger(vClock))
                    {
                        // Clock sync. Measure the most recent time interval in samples.
                        float raw = q.samplesSinceClockTrigger / sampleRateHz;
                        clockSyncTime = std::clamp(raw, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
                        q.samplesSinceClockTrigger = 0;
                    }
                    else
                    {
                        ++q.samplesSinceClockTrigger;
                    }
                    q.loop.setReversed(reversed);
                    if (clearBufferRequested)
                        q.loop.clear();

                    float delayTime;
                    if (clockSyncTime > 0)
                        delayTime = clockSyncTime;      // FIXFIXFIX - use TIME knob as scalar multiplier
                    else
                        delayTime = std::pow(two, controlGroupRawCv(c, cvDelayTime, controls.delayTime, L1, L2));

                    q.loop.setDelayTime(delayTime, sampleRateHz);
                    delayLineOutput.at(c) = q.loop.read();
                }

                // Panning affects all channels at the same time.
                // Therefore it cannot be a polyphonic control.
                // We obtain a single panning signal based on the
                // sum of all polyphonic CV input channels.
                Frame preMixOutput = frozen ? delayLineOutput : panFrame(delayLineOutput);

                // Second pass: send panned audio back into the feedback mixer.
                float fbk = 0;
                float cvMix = 0;
                float cvGain = 0;
                int unhappyCount = 0;
                for (int c = 0; c < nc; ++c)
                {
                    ChannelInfo& q = getChannelInfo(c);

                    if (c < message.feedback.nchannels)
                        fbk = std::clamp<float>(message.feedback.sample[c], 0.0f, 1.0f);

                    float mix = controlGroupAmpCv(c, cvMix, controls.mix, 0, 1);
                    float gain = controlGroupRawCv(c, cvGain, controls.gain, 0, 2);

                    float delayLineInput =
                        frozen
                        ? delayLineOutput.at(c)
                        : inAudio.sample[c] + (fbk * preMixOutput.at(c));

                    // Always write to send ports.
                    // FIXFIXFIX - update later when we support full polyphonic output option on left channels
                    if (c == 0)
                        sendLeft.setVoltage(delayLineInput);
                    else if (c == 1)
                        sendRight.setVoltage(delayLineInput);

                    if (returnLeft.isConnected() || returnRight.isConnected())
                    {
                        // When either RETURN input is connected to a cable,
                        // use the audio input from both as a stereo pair to be
                        // written to the delay line, instead of the raw input
                        // we calculated above. Because we wrote the raw stereo input
                        // to the SEND ports, the user has the opportunity to
                        // pipe the audio through filters, reverbs, etc., before
                        // being fed back into the delay line via the RETURN ports.

                        // FIXFIXFIX - update port input later when we support full polyphonic input option
                        if (c == 0)
                            delayLineInput = returnLeft.getVoltageSum();
                        else if (c == 1)
                            delayLineInput = returnRight.getVoltageSum();
                        else
                            delayLineInput = 0;
                    }

                    if (!q.loop.write(delayLineInput, sampleRateHz))
                        ++unhappyCount;

                    result.outAudio.at(c) = gain * CubicMix(mix, inAudio.at(c), preMixOutput.at(c));
                }

                unhappy = (unhappyCount > 0);
                clearBufferRequested = false;
                return result;
            }

            Frame panFrame(const Frame& rawAudio)
            {
                const int nc = rawAudio.safeChannelCount();
                Frame pannedAudio = rawAudio;

                // We currently support panning on stereo input only.
                // All other channel counts should be left unmodified.
                if (nc == 2)
                {
                    // Use a power law to smoothly transition from A or B dominating,
                    // but with consistent power sum across both channels.
                    float x = getControlValue(controls.pan, -1, +1);
                    float theta = M_PI_4 * (x+1);       // map [-1, +1] onto [0, pi/2]
                    pannedAudio.sample[0] *= M_SQRT2 * std::cos(theta);
                    pannedAudio.sample[1] *= M_SQRT2 * std::sin(theta);
                }

                return pannedAudio;
            }

            void configTimeControls(int paramId, int attenId, int cvInputId)
            {
                const std::string name = "Delay time";
                configParam<TimeKnobParamQuantity>(paramId, L1, L2, 0, name, "", 2, 1);
                configAttenCv(attenId, cvInputId, name);
            }

            void configFeedbackControls(int paramId, int attenId, int cvInputId)
            {
                const std::string name = "Feedback";
                configParam(paramId, 0, 1, 0, name, "%", 0, 100);
                configAttenCv(attenId, cvInputId, name);
            }

            void configPanControls(int paramId, int attenId, int cvInputId)
            {
                const std::string name = "Panning";
                configParam(paramId, -1, +1, 0, name, "%", 0, 100);
                configAttenCv(attenId, cvInputId, name);
            }

            void configMixControls(int paramId, int attenId, int cvInputId)
            {
                const std::string name = "Mix";
                configParam(paramId, 0, 1, 1, name, "%", 0, 100);
                configAttenCv(attenId, cvInputId, name);
            }

            void configGainControls(int paramId, int attenId, int cvInputId)
            {
                const std::string name = "Level";
                configParam(paramId, 0, 1, 1, name, " dB", -10, 20);
                configAttenCv(attenId, cvInputId, name);
            }
        };


        struct LoopWidget : SapphireWidget
        {
            const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");
            const float mmShiftFirstTap = (PanelWidth("echo") - PanelWidth("echotap")) / 2;

            explicit LoopWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            void addExpanderInsertButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createParamCentered<InsertButton>(Vec{}, loopModule, paramId);
                button->loopWidget = this;
                addSapphireParam(button, "insert_button");
            }

            Module* echoReceiverWithinRange()
            {
                if (module == nullptr)
                    return nullptr;

                Module* right = module->rightExpander.module;

                if (IsEchoReceiver(right))
                    return right;

                if (right == nullptr)
                {
                    // There is no module immediately to the right.
                    // Search all modules in the Rack for anything to the right of this module.
                    // Pick the module closest to this one, but only if it is within the width
                    // we need for the hypothetical EchoTap we are about to insert.
                    const int hpEchoTap = hpDistance(PanelWidth("echotap"));
                    assert(hpEchoTap > 0);
                    ModuleWidget* closestWidget = FindWidgetClosestOnRight(this, hpEchoTap);
                    if (closestWidget && IsEchoReceiver(closestWidget->module))
                        return closestWidget->module;
                }

                return nullptr;
            }

            void insertExpander()
            {
                if (module == nullptr)
                    return;

                // We either insert a "EchoTap" module or an "EchoOut" module, depending on the situation.
                // If the module to the right is a EchoTap or an EchoOut, insert another EchoTap.
                // Otherwise, assume we are at the end of a chain that is not terminated by
                // an EchoOut, so insert an EchoOut.

                Module* right = echoReceiverWithinRange();
                Model* model = right ? modelSapphireEchoTap : modelSapphireEchoOut;

                // Erase any obsolete chain indices already in the remaining modules.
                // This prevents them briefly flashing on the screen before being replaced.
                for (Module* node = right; IsEchoReceiver(node); node = node->rightExpander.module)
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
                return module && IsEchoReceiver(module->rightExpander.module);
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

            void drawChainIndex(NVGcontext* vg, int chainIndex, NVGcolor textColor)
            {
                if (module == nullptr)
                    return;

                if (chainIndex < 1)
                    return;

                if (IsEcho(module) && !IsEchoReceiver(module->rightExpander.module))
                    return;

                std::shared_ptr<Font> font = APP->window->loadFont(chainFontPath);
                if (font)
                {
                    const float yCenter_mm = 10.0;

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
                        x1 += mm2px(mmShiftFirstTap);

                    nvgText(vg, x1, y1, text, nullptr);
                }
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);

                auto lmod = dynamic_cast<const LoopModule*>(module);
                if (lmod)
                {
                    if (!lmod->neonMode)
                        drawChainIndex(args.vg, lmod->chainIndex, nvgRGB(0x66, 0x06, 0x5c));
                }
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                SapphireWidget::drawLayer(args, layer);
                if (layer == 1)
                {
                    auto lmod = dynamic_cast<const LoopModule*>(module);
                    if (lmod)
                    {
                        if (lmod->neonMode)
                            drawChainIndex(args.vg, lmod->chainIndex, neonColor);

                        if (lmod->unhappy)
                            splash.begin(0xb0, 0x10, 0x00);
                    }
                }
            }

            void appendContextMenu(Menu *menu) override
            {
                SapphireWidget::appendContextMenu(menu);

                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod != nullptr)
                {
                    menu->addChild(lmod->createToggleAllSensitivityMenuItem());
                }
            }

            TimeKnob* addTimeControlGroup(int paramId, int attenId, int cvInputId)
            {
                TimeKnob* tk = addSapphireFlatControlGroup<TimeKnob>("time", paramId, attenId, cvInputId);
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod != nullptr)
                {
                    tk->mode = &(lmod->timeMode);
                    tk->isClockConnected = &(lmod->isClockConnected);
                }
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


        std::string TimeKnobParamQuantity::getString()
        {
            std::string v = ParamQuantity::getDisplayValueString();
            auto lmod = dynamic_cast<const LoopModule*>(module);
            if (lmod && lmod->isActivelyClocked())
                return "CLOCK x " + v;
            return v + " sec";
        }


        namespace Echo
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

            struct EchoModule : LoopModule
            {
                // Global controls
                GateTriggerReceiver freezeReceiver;
                AnimatedTriggerReceiver clearReceiver;

                EchoModule()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    chainIndex = 1;
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    defineControls();
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
                    EchoModule_initialize();
                }

                void EchoModule_initialize()
                {
                    freezeReceiver.initialize();
                    clearReceiver.initialize();
                }

                void initialize() override
                {
                    LoopModule::initialize();
                    EchoModule_initialize();
                }

                void defineControls()
                {
                    controls.delayTime = ControlGroupIds(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    controls.mix = ControlGroupIds(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    controls.gain = ControlGroupIds(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    controls.pan = ControlGroupIds(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    controls.sendLeftOutputId   = SEND_LEFT_OUTPUT;
                    controls.sendRightOutputId  = SEND_RIGHT_OUTPUT;
                    controls.returnLeftInputId  = RETURN_LEFT_INPUT;
                    controls.returnRightInputId = RETURN_RIGHT_INPUT;
                    controls.clockInputId = CLOCK_INPUT;
                }

                void process(const ProcessArgs& args) override
                {
                    Message outMessage;
                    frozen = outMessage.frozen = updateFreezeState();
                    reversed = updateReverseState();
                    clearBufferRequested = outMessage.clear = updateClearState(args.sampleRate);
                    outMessage.chainIndex = 2;
                    outMessage.originalAudio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);
                    outMessage.feedback = getFeedbackPoly();
                    isClockConnected = outMessage.isClockConnected = inputs.at(CLOCK_INPUT).isConnected();
                    TapeLoopResult result = updateTapeLoops(outMessage.originalAudio, args.sampleRate, outMessage);
                    outMessage.chainAudio = result.outAudio;
                    outMessage.clockVoltage = result.clockVoltage;
                    outMessage.neonMode = neonMode;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, outMessage.chainAudio);
                    sendMessage(outMessage);
                }

                Frame getFeedbackPoly()
                {
                    const float maxFeedbackRatio = 0.97;
                    Frame feedback;
                    Input& cvInput = inputs.at(FEEDBACK_CV_INPUT);
                    const int nc = VcvSafeChannelCount(cvInput.getChannels());
                    feedback.nchannels = std::max(1, nc);
                    float cv = 0;
                    for (int c = 0; c < feedback.nchannels; ++c)
                    {
                        nextChannelInputVoltage(cv, FEEDBACK_CV_INPUT, c);
                        feedback.sample[c] = maxFeedbackRatio * cvGetVoltPerOctave(FEEDBACK_PARAM, FEEDBACK_ATTEN, cv, 0, 1);
                    }
                    return feedback;
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

            struct EchoWidget : LoopWidget
            {
                EchoModule* echoModule{};

                explicit EchoWidget(EchoModule* module)
                    : LoopWidget("echo", asset::plugin(pluginInstance, "res/echo.svg"))
                    , echoModule(module)
                {
                    splash.x1 = 6 * HP_MM;
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

                void addFreezeToggleGroup()
                {
                    addToggleGroup(
                        "freeze",
                        FREEZE_INPUT,
                        FREEZE_BUTTON_PARAM,
                        FREEZE_BUTTON_LIGHT,
                        '\0',
                        0.0,
                        SCHEME_BLUE
                    );
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

        namespace EchoTap
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

            struct EchoTapModule : LoopModule
            {
                EchoTapModule()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    defineControls();
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
                    EchoTapModule_initialize();
                }

                void EchoTapModule_initialize()
                {
                }

                void initialize() override
                {
                    LoopModule::initialize();
                    EchoTapModule_initialize();
                }

                void defineControls()
                {
                    controls.delayTime = ControlGroupIds(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    controls.mix = ControlGroupIds(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);
                    controls.gain = ControlGroupIds(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    controls.pan = ControlGroupIds(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    controls.sendLeftOutputId   = SEND_LEFT_OUTPUT;
                    controls.sendRightOutputId  = SEND_RIGHT_OUTPUT;
                    controls.returnLeftInputId  = RETURN_LEFT_INPUT;
                    controls.returnRightInputId = RETURN_RIGHT_INPUT;
                }

                void process(const ProcessArgs& args) override
                {
                    const Message inMessage = receiveMessageOrDefault();

                    // Copy input to output by default, then patch whatever is different.
                    Message outMessage = inMessage;
                    chainIndex = inMessage.chainIndex;
                    frozen = inMessage.frozen;
                    isClockConnected = inMessage.isClockConnected;
                    includeNeonModeMenuItem = !receivedMessageFromLeft;
                    if (receivedMessageFromLeft)
                        neonMode = inMessage.neonMode;
                    reversed = updateReverseState();
                    clearBufferRequested = inMessage.clear;
                    outMessage.chainIndex = (chainIndex < 0) ? -1 : (1 + chainIndex);
                    TapeLoopResult result = updateTapeLoops(inMessage.chainAudio, args.sampleRate, outMessage);
                    outMessage.chainAudio = result.outAudio;
                    outMessage.clockVoltage = result.clockVoltage;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, outMessage.chainAudio);
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

            struct EchoTapWidget : LoopWidget
            {
                EchoTapModule* echoTapModule{};

                explicit EchoTapWidget(EchoTapModule* module)
                    : LoopWidget("echotap", asset::plugin(pluginInstance, "res/echotap.svg"))
                    , echoTapModule(module)
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
                    return module && IsEchoSender(module->leftExpander.module);
                }
            };
        }

        namespace EchoOut
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

            struct EchoOutModule : MultiTapModule
            {
                EchoOutModule()
                    : MultiTapModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                    configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                    configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                    configControlGroup("Output mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT, 0, 1, 1, "%", 0, 100);
                    configControlGroup("Output level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                    EchoOutModule_initialize();
                }

                void EchoOutModule_initialize()
                {
                }

                void initialize() override
                {
                    MultiTapModule::initialize();
                    EchoOutModule_initialize();
                }

                void process(const ProcessArgs& args) override
                {
                    const Message message = receiveMessageOrDefault();
                    chainIndex = message.chainIndex;

                    includeNeonModeMenuItem = !receivedMessageFromLeft;
                    if (receivedMessageFromLeft)
                        neonMode = message.neonMode;

                    Frame audio;
                    audio.nchannels = VcvSafeChannelCount(
                        std::max(
                            message.chainAudio.nchannels,
                            message.originalAudio.nchannels
                        )
                    );

                    float cvLevel = 0;
                    float cvMix = 0;
                    for (int c = 0; c < audio.nchannels; ++c)
                    {
                        nextChannelInputVoltage(cvLevel, GLOBAL_LEVEL_CV_INPUT, c);
                        float gain = Cube(cvGetVoltPerOctave(GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, cvLevel, 0, 2));

                        nextChannelInputVoltage(cvMix, GLOBAL_MIX_CV_INPUT, c);
                        float mix = cvGetControlValue(GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, cvMix, 0, 1);

                        audio.sample[c] = gain * CubicMix(
                            mix,
                            message.originalAudio.sample[c],
                            message.chainAudio.sample[c]
                        );
                    }

                    Output& audioLeftOutput  = outputs.at(AUDIO_LEFT_OUTPUT);
                    Output& audioRightOutput = outputs.at(AUDIO_RIGHT_OUTPUT);

                    audioLeftOutput.setChannels(1);
                    audioLeftOutput.setVoltage(audio.sample[0], 0);

                    audioRightOutput.setChannels(1);
                    audioRightOutput.setVoltage(audio.sample[1], 0);
                }
            };

            struct EchoOutWidget : SapphireWidget
            {
                EchoOutModule* echoOutModule{};

                explicit EchoOutWidget(EchoOutModule* module)
                    : SapphireWidget("echoout", asset::plugin(pluginInstance, "res/echoout.svg"))
                    , echoOutModule(module)
                {
                    setModule(module);
                    addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                    addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                    addSapphireControlGroup("global_mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT);
                    addSapphireControlGroup("global_level", GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, GLOBAL_LEVEL_CV_INPUT);
                }

                bool isConnectedOnLeft() const
                {
                    return module && IsEchoSender(module->leftExpander.module);
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


Model* modelSapphireEcho = createSapphireModel<Sapphire::MultiTap::Echo::EchoModule, Sapphire::MultiTap::Echo::EchoWidget>(
    "Echo",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireEchoTap = createSapphireModel<Sapphire::MultiTap::EchoTap::EchoTapModule, Sapphire::MultiTap::EchoTap::EchoTapWidget>(
    "EchoTap",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireEchoOut = createSapphireModel<Sapphire::MultiTap::EchoOut::EchoOutModule, Sapphire::MultiTap::EchoOut::EchoOutWidget>(
    "EchoOut",
    Sapphire::ExpanderRole::MultiTap
);
