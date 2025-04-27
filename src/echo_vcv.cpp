#include "sapphire_multitap.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct LoopModule;
        struct LoopWidget;

        namespace Echo
        {
            struct EchoWidget;
        }

        enum class TimeMode
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
                menu->addChild(createEnumMenuItem(
                    "Time mode",
                    {
                        "Seconds",
                        "Clock sync"
                    },
                    *mode
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


        using clock_button_base_t = app::SvgSwitch;
        struct ClockButton : clock_button_base_t
        {
            Echo::EchoWidget* echoWidget{};

            explicit ClockButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_1.svg")));
            }

            void onButton(const ButtonEvent& e) override;
        };


        struct ReverseButton : SapphireCaptionButton
        {
            LoopModule* loopModule{};

            ReverseOutput getReverseOutputMode() const;
            void appendContextMenu(Menu* menu) override;
            void drawLayer(const DrawArgs& args, int layer) override;
            void onButton(const ButtonEvent& e) override;
        };


        struct MultiTapModule : SapphireModule
        {
            Message messageBuffer[2];
            BackwardMessage backwardMessageBuffer[2];
            int chainIndex = -1;
            bool receivedMessageFromLeft = false;

            explicit MultiTapModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
            {
                rightExpander.producerMessage = &messageBuffer[0];
                rightExpander.consumerMessage = &messageBuffer[1];

                leftExpander.producerMessage = &backwardMessageBuffer[0];
                leftExpander.consumerMessage = &backwardMessageBuffer[1];

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

            void sendBackwardMessage(const BackwardMessage& backMessage)
            {
                BackwardMessage& destination = *static_cast<BackwardMessage*>(leftExpander.producerMessage);
                destination = backMessage;
                leftExpander.requestMessageFlip();
            }

            const BackwardMessage* receiveBackwardMessage()
            {
                if (IsEchoTap(rightExpander.module))
                    return static_cast<const BackwardMessage*>(rightExpander.module->leftExpander.consumerMessage);
                return nullptr;
            }

            BackwardMessage receiveBackwardMessageOrDefault()
            {
                const BackwardMessage* ptr = receiveBackwardMessage();
                return ptr ? *ptr : BackwardMessage{};
            }

            void writeSample(float voltage, Output& outLeft, Output& outRight, int c, int nc, bool polyphonic)
            {
                if (nc==2 && !polyphonic)
                {
                    // Stereo output.
                    if (c == 0)
                    {
                        outLeft.setChannels(1);
                        outLeft.setVoltage(voltage);
                    }
                    else if (c == 1)
                    {
                        outRight.setVoltage(voltage);
                    }
                }
                else if (nc > 0)
                {
                    // Polyphonic output.
                    outLeft.setChannels(nc);
                    if (0 <= c && c < nc)
                        outLeft.setVoltage(voltage, c);
                }
            }

            float readSample(float normal, Input& inLeft, Input& inRight, int c)
            {
                if (inLeft.isConnected())
                {
                    if (inRight.isConnected())
                    {
                        // Stereo input
                        if (c == 0)
                            return inLeft.getVoltageSum();
                        if (c == 1)
                            return inRight.getVoltageSum();
                        return 0;
                    }

                    const int ncLeft = inLeft.getChannels();
                    if (ncLeft > 1)
                    {
                        // Polyphonic input
                        if (0 <= c && c < ncLeft)
                            return inLeft.getVoltage(c);
                        return 0;
                    }

                    // Mono input, so split the signal across both stereo output channels.
                    if (c==0 || c==1)
                        return inLeft.getVoltageSum() / 2;
                    return 0;
                }
                return normal;
            }

            bool isPolyphonic(int leftInputId, int rightInputId)
            {
                Input& inLeft  = inputs.at(leftInputId);
                Input& inRight = inputs.at(rightInputId);
                return !inRight.isConnected() && (inLeft.getChannels() > 1);
            }

            Frame readFrame(int leftInputId, int rightInputId)
            {
                Input& inLeft  = inputs.at(leftInputId);
                Input& inRight = inputs.at(rightInputId);

                Frame frame;
                frame.nchannels = 2;
                frame.sample[0] = inLeft.getVoltageSum();
                frame.sample[1] = inRight.getVoltageSum();

                if (!inRight.isConnected())
                {
                    const int ncLeft = inLeft.getChannels();
                    if (ncLeft == 1)
                    {
                        // Mono input, so split the signal across both stereo output channels.
                        frame.sample[0] /= 2;
                        frame.sample[1] = frame.sample[0];
                    }
                    else if (ncLeft > 1)
                    {
                        // Polyphonic mode!
                        frame.nchannels = VcvSafeChannelCount(ncLeft);
                        for (int c = 0; c < frame.nchannels; ++c)
                            frame.sample[c] = inLeft.getVoltage(c);
                    }
                }

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
            ReverseOutput reverseOutput = ReverseOutput::Mix;
            TapInputRouting receivedInputRouting{};

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
                reverseOutput = ReverseOutput::Mix;
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
                jsonSetEnum(root, "timeMode", timeMode);
                jsonSetEnum(root, "reverseOutput", reverseOutput);
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                MultiTapModule::dataFromJson(root);
                jsonLoadEnum(root, "timeMode", timeMode);
                jsonLoadEnum(root, "reverseOutput", reverseOutput);
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
                Frame globalAudioOutput;
                Frame chainAudioOutput;
                Frame clockVoltage;
            };

            virtual void bumpTapInputRouting()
            {
                // Do nothing... only EchoModule uses this.
            }

            TapeLoopResult updateTapeLoops(
                const Frame& inAudio,
                float sampleRateHz,
                const Message& message,
                const BackwardMessage& backMessage)
            {
                receivedInputRouting = message.inputRouting;

                const int nc = inAudio.safeChannelCount();
                const float two = 2;    // silly, but helps for portability to MAC/ARM

                Output& sendLeft   = outputs.at(controls.sendLeftOutputId);
                Output& sendRight  = outputs.at(controls.sendRightOutputId);
                Input& returnLeft  = inputs.at(controls.returnLeftInputId);
                Input& returnRight = inputs.at(controls.returnRightInputId);

                TapeLoopResult result;
                result.globalAudioOutput.nchannels = nc;
                result.chainAudioOutput.nchannels = nc;
                result.clockVoltage.nchannels = nc;

                Frame reversibleDelayLineOutput;
                reversibleDelayLineOutput.nchannels = nc;

                float cvDelayTime = 0;
                float vClock = 0;
                float fbk = 0;
                float cvGain = 0;
                int unhappyCount = 0;

                bool allowFeedback;
                switch (message.inputRouting)
                {
                case TapInputRouting::Serial:
                    allowFeedback = !IsEchoTap(rightExpander.module);
                    break;

                case TapInputRouting::Loop:
                    allowFeedback = IsEcho(this);
                    break;

                case TapInputRouting::Parallel:
                default:
                    allowFeedback = true;
                    break;
                }

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
                        delayTime = clockSyncTime;
                    else
                        delayTime = std::pow(two, controlGroupRawCv(c, cvDelayTime, controls.delayTime, L1, L2));

                    q.loop.setDelayTime(delayTime, sampleRateHz);
                    q.loop.setInterpolatorKind(message.interpolatorKind);
                    TapeLoopReadResult rr = q.loop.read();
                    reversibleDelayLineOutput.at(c) = rr.playback;

                    float feedbackSample = rr.feedback;
                    if (allowFeedback)
                    {
                        if (c < message.feedback.nchannels)
                            fbk = std::clamp<float>(message.feedback.sample[c], 0.0f, 1.0f);

                        if (message.inputRouting == TapInputRouting::Loop)
                        {
                            if (c < backMessage.loopAudio.nchannels)
                                feedbackSample = backMessage.loopAudio.sample[c];
                            else
                                feedbackSample = 0;
                        }
                    }

                    float gain = controlGroupRawCv(c, cvGain, controls.gain, 0, 2);

                    float delayLineInput =
                        frozen
                        ? feedbackSample
                        : inAudio.sample[c] + (fbk * feedbackSample);

                    writeSample(delayLineInput, sendLeft, sendRight, c, nc, message.polyphonic);
                    delayLineInput = readSample(delayLineInput, returnLeft, returnRight, c);

                    if (!q.loop.write(delayLineInput))
                        ++unhappyCount;

                    switch (reverseOutput)
                    {
                    case ReverseOutput::Mix:
                    default:
                        result.chainAudioOutput.at(c) = rr.feedback;
                        break;

                    case ReverseOutput::MixAndChain:
                        result.chainAudioOutput.at(c) = reversibleDelayLineOutput.at(c);
                        break;
                    }

                    result.globalAudioOutput.at(c) = gain * reversibleDelayLineOutput.at(c);
                }

                result.globalAudioOutput = panFrame(result.globalAudioOutput);
                clearBufferRequested = false;
                unhappy = (unhappyCount > 0);
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

            void configGainControls(int paramId, int attenId, int cvInputId)
            {
                const std::string name = "Level";
                configParam(paramId, 0, 1, 1, name, " dB", -10, 20);
                configAttenCv(attenId, cvInputId, name);
            }

            MenuItem* createReverseOutputMenuItem()
            {
                return createEnumMenuItem(
                    "Output to:",
                    {
                        "Mixer",
                        "Mixer + next tap",
                    },
                    reverseOutput
                );
            }
        };


        ReverseOutput ReverseButton::getReverseOutputMode() const
        {
            return loopModule ? loopModule->reverseOutput : ReverseOutput::Mix;
        }


        void ReverseButton::appendContextMenu(Menu* menu)
        {
            if (loopModule)
                menu->addChild(loopModule->createReverseOutputMenuItem());
        }


        void ReverseButton::drawLayer(const DrawArgs& args, int layer)
        {
            if (layer == 1)
            {
                switch (getReverseOutputMode())
                {
                case ReverseOutput::Mix:
                    caption[0] = '\0';
                    break;

                case ReverseOutput::MixAndChain:
                    caption[0] = '>';
                    dxText = 9.0;
                    dyText = 8.5;
                    break;

                default:
                    caption[0] = '?';
                    dxText = 7.0;
                    dyText = 9.5;
                    break;
                }
            }
            SapphireCaptionButton::drawLayer(args, layer);
        }


        void ReverseButton::onButton(const ButtonEvent& e)
        {
            if (loopModule)
            {
                // If the user holds SHIFT while clicking the button,
                // toggle the reverse-output mode.
                if ((e.mods & GLFW_MOD_SHIFT) && (e.action == GLFW_PRESS) && (e.button == GLFW_MOUSE_BUTTON_LEFT))
                {
                    loopModule->reverseOutput = NextEnumValue(loopModule->reverseOutput);
                }
            }

            SapphireCaptionButton::onButton(e);
        }


        struct LoopWidget : SapphireWidget
        {
            const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");
            const float mmShiftFirstTap = (PanelWidth("echo") - PanelWidth("echotap")) / 2;
            const float mmModeButtonRadius = 3.5;
            const float mmChainIndexCenterY = 4.5;
            bool hilightInputRoutingButton = false;

            explicit LoopWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            void addExpanderInsertButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createParamCentered<InsertButton>(Vec{}, loopModule, paramId);
                button->loopWidget = this;
                addSapphireParam(button, "insert_button");
            }

            void addReverseToggleGroup(int inputId, int buttonParamId, int buttonLightId)
            {
                auto reverseButton = addToggleGroup<ReverseButton>(
                    "reverse",
                    inputId,
                    buttonParamId,
                    buttonLightId,
                    '\0',
                    7.0,
                    SCHEME_PURPLE
                );

                reverseButton->loopModule = dynamic_cast<LoopModule*>(module);
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

            void drawCenteredText(NVGcontext* vg, float xCenter, float yCenter, const char *text)
            {
                float bounds[4]{};
                nvgTextBounds(vg, 0, 0, text, nullptr, bounds);
                // "L" => bounds=[0.000000, -14.353189, 9.333333, 3.646812]
                //                xmin       ymin       xmax      ymax
                float width = bounds[2] - bounds[0];
                float ascent = -bounds[1];
                nvgText(vg, xCenter - width/2, yCenter + ascent/2, text, nullptr);
            }

            Vec getChainIndexCenterPos() const
            {
                float yCenter = mm2px(mmChainIndexCenterY);
                float xCenter = box.size.x/2;
                if (IsEcho(module))
                    xCenter += mm2px(mmShiftFirstTap);
                return Vec(xCenter, yCenter);
            }

            Vec getTapInputRoutingPos() const
            {
                float yCenter = mm2px(mmChainIndexCenterY);
                float xCenter = mm2px(FindComponent(modcode, "reverse_input").cx);
                return Vec(xCenter, yCenter);
            }

            bool isInsideInputRoutingButton(Vec pos) const
            {
                if (!IsEcho(module))
                    return false;
                Vec center = getTapInputRoutingPos();
                float distance = center.minus(pos).norm();
                return distance <= mm2px(mmModeButtonRadius);
            }

            void onMousePress(const ButtonEvent& e)
            {
            }

            void onMouseRelease(const ButtonEvent& e)
            {
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    if (isInsideInputRoutingButton(e.pos))
                        lmod->bumpTapInputRouting();
                }
            }

            void onButton(const ButtonEvent& e) override
            {
                if (e.button == GLFW_MOUSE_BUTTON_LEFT)
                {
                    switch (e.action)
                    {
                    case GLFW_PRESS:    onMousePress(e);     break;
                    case GLFW_RELEASE:  onMouseRelease(e);   break;
                    }
                }
                SapphireWidget::onButton(e);
            }

            void onHover(const HoverEvent& e) override
            {
                hilightInputRoutingButton = isInsideInputRoutingButton(e.pos);
                SapphireWidget::onHover(e);
            }

            void onLeave(const LeaveEvent& e) override
            {
                hilightInputRoutingButton = false;
                SapphireWidget::onLeave(e);
            }

            void drawChainIndex(
                NVGcontext* vg,
                int chainIndex,
                TapInputRouting routing,
                NVGcolor textColor)
            {
                if (module == nullptr)
                    return;

                std::shared_ptr<Font> font = APP->window->loadFont(chainFontPath);
                if (!font)
                    return;

                nvgFontSize(vg, 18);
                nvgFontFaceId(vg, font->handle);
                nvgFillColor(vg, textColor);

                const bool isEcho = IsEcho(module);
                const bool isDisconnectedEcho = isEcho && !IsEchoReceiver(module->rightExpander.module);
                const bool shouldDrawNumber = (chainIndex > 0) && !isDisconnectedEcho;

                char text[20];
                if (shouldDrawNumber)
                {
                    snprintf(text, sizeof(text), "%d", chainIndex);
                    Vec center = getChainIndexCenterPos();
                    drawCenteredText(vg, center.x, center.y, text);
                }

                if (isEcho)
                {
                    text[0] = InputRoutingChar(routing);
                    text[1] = '\0';
                    Vec center = getTapInputRoutingPos();
                    drawCenteredText(vg, center.x, center.y, text);
                    if (hilightInputRoutingButton)
                    {
                        const float mmAdjustY = 0.4;

                        nvgBeginPath(vg);
                        nvgStrokeColor(vg, textColor);
                        nvgStrokeWidth(vg, 1.0);
                        nvgCircle(vg, center.x, center.y + mm2px(mmAdjustY), mm2px(mmModeButtonRadius));
                        nvgStroke(vg);
                    }
                }
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);

                auto lmod = dynamic_cast<const LoopModule*>(module);
                if (lmod)
                {
                    if (!lmod->neonMode)
                        drawChainIndex(args.vg, lmod->chainIndex, lmod->receivedInputRouting, nvgRGB(0x66, 0x06, 0x5c));
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
                            drawChainIndex(args.vg, lmod->chainIndex, lmod->receivedInputRouting, neonColor);

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
                DC_REJECT_PARAM,
                _OBSOLETE_PARAM,
                GAIN_PARAM,
                GAIN_ATTEN,
                REVERSE_BUTTON_PARAM,
                FREEZE_BUTTON_PARAM,
                CLEAR_BUTTON_PARAM,
                ENV_GAIN_PARAM,
                CLOCK_BUTTON_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                AUDIO_LEFT_INPUT,
                AUDIO_RIGHT_INPUT,
                TIME_CV_INPUT,
                FEEDBACK_CV_INPUT,
                PAN_CV_INPUT,
                _OBSOLETE_INPUT,
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
                TapInputRouting tapInputRouting{};
                InterpolatorKind interpolatorKind{};

                using dc_reject_t = StagedFilter<float, 3>;
                dc_reject_t inputFilter[PORT_MAX_CHANNELS];

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
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    configToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, "Reverse", "Reverse");
                    configToggleGroup(FREEZE_INPUT, FREEZE_BUTTON_PARAM, "Freeze", "Freeze");
                    configToggleGroup(CLEAR_INPUT, CLEAR_BUTTON_PARAM, "Clear", "Clear");
                    configInput(CLOCK_INPUT, "Clock");
                    configButton(CLOCK_BUTTON_PARAM, "Toggle all clock sync");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    addDcRejectQuantity(DC_REJECT_PARAM, 20);
                    EchoModule_initialize();
                }

                void EchoModule_initialize()
                {
                    tapInputRouting = TapInputRouting::Parallel;
                    interpolatorKind = InterpolatorKind::Linear;
                    freezeReceiver.initialize();
                    clearReceiver.initialize();
                    dcRejectQuantity->initialize();
                    params.at(REVERSE_BUTTON_PARAM).setValue(0);
                    params.at(FREEZE_BUTTON_PARAM).setValue(0);
                    params.at(CLEAR_BUTTON_PARAM).setValue(0);
                }

                void initialize() override
                {
                    LoopModule::initialize();
                    EchoModule_initialize();
                }

                void defineControls()
                {
                    controls.delayTime = ControlGroupIds(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
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
                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();
                    outMessage.inputRouting = tapInputRouting;
                    outMessage.polyphonic = isPolyphonic(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);
                    frozen = outMessage.frozen = updateFreezeState();
                    reversed = updateReverseState();
                    clearBufferRequested = outMessage.clear = updateClearState(args.sampleRate);
                    outMessage.chainIndex = 2;
                    outMessage.originalAudio = readOriginalAudio(args.sampleRate);
                    outMessage.feedback = getFeedbackPoly();
                    isClockConnected = outMessage.isClockConnected = inputs.at(CLOCK_INPUT).isConnected();
                    outMessage.interpolatorKind = interpolatorKind;
                    TapeLoopResult result = updateTapeLoops(outMessage.originalAudio, args.sampleRate, outMessage, inBackMessage);
                    outMessage.chainAudio = result.chainAudioOutput;
                    outMessage.summedAudio = result.globalAudioOutput;
                    outMessage.clockVoltage = result.clockVoltage;
                    outMessage.neonMode = neonMode;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, outMessage.chainAudio);
                    sendMessage(outMessage);
                }

                Frame readOriginalAudio(float sampleRateHz)
                {
                    Frame audio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);
                    const int nc = audio.safeChannelCount();
                    for (int c = 0; c < nc; ++c)
                    {
                        inputFilter[c].SetCutoffFrequency(dcRejectQuantity->value);
                        audio.sample[c] = inputFilter[c].UpdateHiPass(audio.sample[c], sampleRateHz);
                    }
                    return audio;
                }

                json_t* dataToJson() override
                {
                    json_t* root = LoopModule::dataToJson();
                    jsonSetEnum(root, "tapInputRouting", tapInputRouting);
                    jsonSetEnum(root, "interpolatorKind", interpolatorKind);
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    LoopModule::dataFromJson(root);
                    jsonLoadEnum(root, "tapInputRouting", tapInputRouting);
                    jsonLoadEnum(root, "interpolatorKind", interpolatorKind);
                }

                Frame getFeedbackPoly()
                {
                    // We don't actually allow 100% feedback because it
                    // is completely unstable and will most often over-volt the record head.
                    // So scale to maxFeedbackRatio as the true feedback ratio.
                    const float maxFeedbackRatio = 0.9;
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

                void bumpTapInputRouting() override
                {
                    // FIXFIXFIX_ANTICLICK : fix this in the TapeLoop class, just like we did for reverse toggle.
                    tapInputRouting = NextEnumValue(tapInputRouting);
                }
            };

            struct EchoWidget : LoopWidget
            {
                EchoModule* echoModule{};
                int creationCountdown = 8;

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
                    addClockButton();

                    // Per-tap controls/ports
                    addStereoOutputPorts(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    addStereoInputPorts(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    addTimeControlGroup(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    addReverseToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    addSapphireOutput(ENV_OUTPUT, "env_output");
                    addSmallKnob(ENV_GAIN_PARAM, "env_gain_knob");
                }

                void addClockButton()
                {
                    auto button = createParamCentered<ClockButton>(Vec{}, echoModule, CLOCK_BUTTON_PARAM);
                    button->echoWidget = this;
                    addSapphireParam(button, "clock_button");
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

                void drawClockSyncSymbol(NVGcontext* vg, NVGcolor color, float strokeWidth)
                {
                    ComponentLocation clock = FindComponent(modcode, "clock_input");

                    float dx = 6.0;
                    float dy = 4.5;
                    float x1 = mm2px(clock.cx - dx);
                    float x2 = mm2px(clock.cx + dx);
                    float y1 = mm2px(clock.cy - dy);
                    float y2 = mm2px(clock.cy + dy);

                    nvgBeginPath(vg);
                    nvgStrokeColor(vg, color);
                    nvgMoveTo(vg, x1, y1);
                    nvgLineTo(vg, x2, y1);
                    nvgLineTo(vg, x2, y2);
                    nvgLineTo(vg, x1, y2);
                    nvgLineTo(vg, x1, y1);
                    nvgStrokeWidth(vg, strokeWidth);
                    nvgStroke(vg);
                }

                bool isClockPortConnected()
                {
                    return echoModule && echoModule->inputs.at(CLOCK_INPUT).isConnected();
                }

                void drawLayer(const DrawArgs& args, int layer) override
                {
                    // There is a rectangle around the CLOCK input port.
                    // This rectangle should glow when one or more taps
                    // have an active clock sync.
                    // Otherwise it should be opaque black on the panel layer.
                    LoopWidget::drawLayer(args, layer);
                    if (layer == 1 && isClockPortConnected())
                        drawClockSyncSymbol(args.vg, SCHEME_CYAN, 1.5);
                }

                void draw(const DrawArgs& args) override
                {
                    LoopWidget::draw(args);
                    if (!isClockPortConnected())
                        drawClockSyncSymbol(args.vg, SCHEME_BLACK, 1.0);
                }

                void step() override
                {
                    LoopWidget::step();

                    // Automatically add an EchoOut expander when we first insert Echo.
                    // But we have to wait more than one step call, because otherwise
                    // it screws up the undo/redo history stack.

                    if (module && creationCountdown > 0)
                    {
                        --creationCountdown;
                        if (creationCountdown == 0)
                            if (!IsEchoReceiver(module->rightExpander.module))
                                AddExpander(modelSapphireEchoOut, this, ExpanderDirection::Right);
                    }
                }

                void appendContextMenu(Menu* menu) override
                {
                    LoopWidget::appendContextMenu(menu);
                    if (echoModule)
                    {
                        menu->addChild(new MenuSeparator);

                        menu->addChild(createEnumMenuItem(      // FIXFIXFIX_ANTICLICK
                            "Tap input routing",
                            {
                                "Serial",
                                "Parallel",
                                "Loop"
                            },
                            echoModule->tapInputRouting
                        ));

                        menu->addChild(createEnumMenuItem(
                            "Interpolator",
                            {
                                "Linear (uses less CPU)",
                                "Sinc (cleaner audio)"
                            },
                            echoModule->interpolatorKind
                        ));

                        menu->addChild(createMenuItem(
                            "Initialize Echo expander chain",
                            "",
                            [=]{ initializeExpanderChain(); }
                        ));

                        menu->addChild(createMenuItem(
                            "Toggle all clock sync",
                            "",
                            [=]{ toggleAllClockSync(); }
                        ));
                    }
                }

                void initializeExpanderChain()
                {
                    if (echoModule)
                    {
                        // We need to create a history item for the undo/redo stack.
                        // This item has to remember the json-serialized form of each module
                        // before we reset it. We do not need to remember the reset state, because
                        // we can always reset again!

                        std::vector<InitChainNode> list;
                        list.push_back(InitChainNode(echoModule));
                        APP->engine->resetModule(echoModule);

                        Module* module = echoModule->rightExpander.module;
                        while (IsEchoReceiver(module))
                        {
                            list.push_back(InitChainNode(module));
                            APP->engine->resetModule(module);
                            module = module->rightExpander.module;
                        }

                        APP->history->push(new InitChainAction(list));
                    }
                }

                void toggleAllClockSync()
                {
                    if (echoModule)
                    {
                        int totalCount = 1;
                        int clockCount = 0;
                        if (echoModule->timeMode == TimeMode::ClockSync)
                            ++clockCount;

                        Module* module = echoModule->rightExpander.module;
                        while (IsEchoReceiver(module))
                        {
                            auto lmod = dynamic_cast<const LoopModule*>(module);
                            if (lmod)
                            {
                                ++totalCount;
                                if (lmod->timeMode == TimeMode::ClockSync)
                                    ++clockCount;
                            }
                            module = module->rightExpander.module;
                        }

                        echoModule->timeMode =
                            (2*clockCount > totalCount) ?
                            TimeMode::Seconds :
                            TimeMode::ClockSync;

                        module = echoModule->rightExpander.module;
                        while (IsEchoReceiver(module))
                        {
                            auto lmod = dynamic_cast<LoopModule*>(module);
                            if (lmod)
                                lmod->timeMode = echoModule->timeMode;
                            module = module->rightExpander.module;
                        }
                    }
                }
            };
        }

        void ClockButton::onButton(const ButtonEvent& e)
        {
            if (echoWidget)
            {
                if (e.button == GLFW_MOUSE_BUTTON_LEFT && (e.action == GLFW_PRESS))
                    echoWidget->toggleAllClockSync();
            }
            clock_button_base_t::onButton(e);
        }

        void InitChainAction::undo()
        {
            for (const InitChainNode& node : list)
            {
                Module* module = APP->engine->getModule(node.moduleId);
                if (module)
                    APP->engine->moduleFromJson(module, node.json);
            }
        }

        void InitChainAction::redo()
        {
            for (const InitChainNode& node : list)
            {
                Module* module = APP->engine->getModule(node.moduleId);
                if (module)
                    APP->engine->resetModule(module);
            }
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
                OBSOLETE_MIX_PARAM,
                OBSOLETE_MIX_ATTEN,
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
                OBSOLETE_MIX_CV_INPUT,
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
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    configToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, "Reverse", "Reverse");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    EchoTapModule_initialize();
                }

                void EchoTapModule_initialize()
                {
                    params.at(REVERSE_BUTTON_PARAM).setValue(0);
                }

                void initialize() override
                {
                    LoopModule::initialize();
                    EchoTapModule_initialize();
                }

                void defineControls()
                {
                    controls.delayTime = ControlGroupIds(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
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
                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();

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

                    Frame tapInputAudio;
                    switch (inMessage.inputRouting)
                    {
                    case TapInputRouting::Serial:
                    default:
                        tapInputAudio = inMessage.chainAudio;
                        break;

                    case TapInputRouting::Parallel:
                        tapInputAudio = inMessage.originalAudio;
                        break;
                    }

                    TapeLoopResult result = updateTapeLoops(tapInputAudio, args.sampleRate, outMessage, inBackMessage);
                    outMessage.chainAudio = result.chainAudioOutput;
                    outMessage.summedAudio += result.globalAudioOutput;
                    outMessage.clockVoltage = result.clockVoltage;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, outMessage.chainAudio);
                    sendMessage(outMessage);

                    BackwardMessage outBackMessage;
                    if (IsEchoTap(rightExpander.module))
                        outBackMessage = inBackMessage;
                    else
                        outBackMessage.loopAudio = result.chainAudioOutput;
                    sendBackwardMessage(outBackMessage);
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
                    addReverseToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
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
                    configControlGroup("Output mix", GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, GLOBAL_MIX_CV_INPUT, 0, 1, 0.5, "%", 0, 100);
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

                        audio.sample[c] = gain * LinearMix(
                            mix,
                            message.originalAudio.sample[c],
                            message.summedAudio.sample[c]
                        );
                    }

                    Output& audioLeftOutput  = outputs.at(AUDIO_LEFT_OUTPUT);
                    Output& audioRightOutput = outputs.at(AUDIO_RIGHT_OUTPUT);
                    if (!message.polyphonic && audio.nchannels == 2)
                    {
                        audioLeftOutput.setChannels(1);
                        audioLeftOutput.setVoltage(audio.sample[0], 0);

                        audioRightOutput.setChannels(1);
                        audioRightOutput.setVoltage(audio.sample[1], 0);
                    }
                    else
                    {
                        // Polyphonic output to the L port only.
                        audioLeftOutput.setChannels(audio.nchannels);
                        for (int c = 0; c < audio.nchannels; ++c)
                            audioLeftOutput.setVoltage(audio.sample[c], c);

                        audioRightOutput.setChannels(1);
                        audioRightOutput.setVoltage(0, 0);
                    }
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
