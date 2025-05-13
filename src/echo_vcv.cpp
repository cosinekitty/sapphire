#include <stdexcept>
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
            LEN
        };

        using time_knob_base_t = RoundSmallBlackKnob;
        struct TimeKnob : time_knob_base_t
        {
            TimeMode* mode = nullptr;     // should point into the module, when the module exists
            bool* isClockConnected = nullptr;
            bool* isMusicalInterval = nullptr;

            TimeMode getMode() const
            {
                return mode ? *mode : TimeMode::Seconds;
            }

            bool isClockCableConnected() const
            {
                return isClockConnected && *isClockConnected;
            }

            bool snapMusicalIntervals() const
            {
                return isMusicalInterval && *isMusicalInterval;
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
                    if (snapMusicalIntervals())
                        drawIntervalSnapDot(args.vg);
                    break;
                }
            }

            void drawIntervalSnapDot(NVGcontext* vg)
            {
                float xc = box.size.x/2;
                float yc = 8.0 + (box.size.y/2);
                const NVGcolor color = SCHEME_ORANGE;

                nvgBeginPath(vg);
                nvgCircle(vg, xc, yc, 0.5);
                nvgStrokeColor(vg, color);
                nvgStrokeWidth(vg, 1.25);
                nvgStroke(vg);
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

                const NVGcolor inactiveColor = nvgRGB(0x90, 0x90, 0x90);
                const NVGcolor color = isClockCableConnected() ? SCHEME_CYAN : inactiveColor;

                nvgBeginPath(vg);
                nvgMoveTo(vg, x1, y1);
                nvgLineTo(vg, x1, y2);
                nvgLineTo(vg, x3, y2);
                nvgLineTo(vg, x3, y1);
                nvgLineTo(vg, x1, y1);
                nvgStrokeColor(vg, color);
                nvgStrokeWidth(vg, 1.25);
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

        using remove_button_base_t = app::SvgSwitch;
        struct RemoveButton : remove_button_base_t
        {
            LoopWidget* loopWidget{};

            explicit RemoveButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/remove_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };


        using clock_button_base_t = app::SvgSwitch;
        struct ClockButton : clock_button_base_t
        {
            Echo::EchoWidget* echoWidget{};
            Stopwatch stopwatch;

            explicit ClockButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_1.svg")));
            }

            void onButton(const ButtonEvent& e) override;
            void step() override;
        };


        using interval_button_base_t = app::SvgSwitch;
        struct IntervalButton : interval_button_base_t
        {
            explicit IntervalButton()
            {
                momentary = false;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/interval_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/interval_button_1.svg")));
            }
        };


        using init_chain_button_base_t = app::SvgSwitch;
        struct InitChainButton : init_chain_button_base_t
        {
            Echo::EchoWidget* echoWidget = nullptr;

            explicit InitChainButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_1.svg")));
            }

            void onButton(const ButtonEvent& e) override;
        };


        using init_tap_button_base_t = app::SvgSwitch;
        struct InitTapButton : init_tap_button_base_t
        {
            LoopWidget* loopWidget = nullptr;

            explicit InitTapButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_1.svg")));
            }

            void onButton(const ButtonEvent& e) override;
        };


        using sendreturn_button_base_t = app::SvgSwitch;
        struct SendReturnButton : sendreturn_button_base_t
        {
            LoopWidget* loopWidget{};

            explicit SendReturnButton()
            {
                momentary = false;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_1.svg")));
            }
        };


        struct MultiTapModule : SapphireModule
        {
            Message messageBuffer[2];
            BackwardMessage backwardMessageBuffer[2];
            int chainIndex = -1;
            float pendingMoveX{};
            int pendingMoveStepCount = 0;

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
                destination.valid = true;
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
                return ptr ? *ptr : Message{};
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

            Frame readFrame(int leftInputId, int rightInputId, PortLabelMode& mode)
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
                    if (ncLeft == 1)
                    {
                        // Mono input, so split the signal across both stereo output channels.
                        frame.sample[0] /= 2;
                        frame.sample[1] = frame.sample[0];
                        mode = PortLabelMode::Mono;
                    }
                    else if (ncLeft > 1)
                    {
                        // Polyphonic mode!
                        frame.nchannels = VcvSafeChannelCount(ncLeft);
                        for (int c = 0; c < frame.nchannels; ++c)
                            frame.sample[c] = inLeft.getVoltage(c);
                        mode = static_cast<PortLabelMode>(ncLeft);
                    }
                }

                return frame;
            }

            void beginMoveChain(float x)
            {
                pendingMoveX = x;
                pendingMoveStepCount = 2;
            }
        };


        struct MultiTapWidget : SapphireWidget
        {
            const std::string portLabelFontPath = asset::system("res/fonts/DejaVuSans.ttf");

            explicit MultiTapWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            void step() override
            {
                SapphireWidget::step();

                auto mmod = dynamic_cast<MultiTapModule*>(module);
                if (mmod && OneShotCountdown(mmod->pendingMoveStepCount))
                {
                    // Make a list of all the widgets on the same row, here and to the right.
                    std::vector<MultiTapWidget*> widgetsToRight;
                    for (Widget* w : APP->scene->rack->getModuleContainer()->children)
                    {
                        auto other = dynamic_cast<MultiTapWidget*>(w);
                        if (other && other->box.pos.y == box.pos.y && other->box.pos.x >= box.pos.x)
                            widgetsToRight.push_back(other);
                    }

                    // Make an ordered list of all the remaining widgets in the chain, before moving anything.
                    std::vector<MultiTapWidget*> widgetsInOrder;
                    for (const Module* node = mmod; IsEchoReceiver(node); node = node->rightExpander.module)
                    {
                        auto otherModule =  dynamic_cast<const MultiTapModule*>(node);
                        if (otherModule)
                        {
                            for (MultiTapWidget* otherWidget : widgetsToRight)
                                if (otherWidget->module == otherModule)
                                    widgetsInOrder.push_back(otherWidget);
                        }
                    }

                    // Try to move everyone!

                    std::vector<PanelState> panelsInOrder;

                    const float dx = mmod->pendingMoveX - box.pos.x;
                    for (MultiTapWidget* w : widgetsInOrder)
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

            void drawAudioPortLabels(
                NVGcontext* vg,
                PortLabelMode mode,
                float xCenter_mm,
                float yCenterL_mm,
                float yCenterR_mm)
            {
                char left[3]{};
                char right[3]{};

                switch (mode)
                {
                case PortLabelMode::Mono:
                    left[0] = 'M';
                    break;

                case PortLabelMode::Stereo:
                    left[0] = 'L';
                    right[0] = 'R';
                    break;

                default:
                    const int nc = static_cast<int>(mode);
                    if (nc >= 1 && nc <= 9)
                    {
                        left[0] = '0' + nc;
                    }
                    else if (nc >= 10 && nc <= 16)
                    {
                        left[0] = '1';
                        left[1] = '0' + (nc % 10);
                    }
                    else
                    {
                        left[0] = '?';
                    }
                    break;
                }

                std::shared_ptr<Font> font = APP->window->loadFont(portLabelFontPath);
                if (!font)
                    return;

                nvgFontSize(vg, 12);
                nvgFontFaceId(vg, font->handle);
                nvgFillColor(vg, SCHEME_BLACK);

                float yNudge = -0.3;
                float xc = mm2px(xCenter_mm);
                float yL = mm2px(yCenterL_mm + yNudge);
                float yR = mm2px(yCenterR_mm + yNudge);

                drawCenteredText(vg, xc, yL, left);

                if (right[0])
                    drawCenteredText(vg, xc, yR, right);
            }
        };


        struct BoolToggleAction : history::Action
        {
            bool& flag;
            bool& dirty;

            explicit BoolToggleAction(bool& _flag, bool& _dirty)
                : flag(_flag)
                , dirty(_dirty)
                {}

            void toggle()
            {
                flag = !flag;
                dirty = true;
            }

            void undo() override
            {
                toggle();
            }

            void redo() override
            {
                toggle();
            }
        };


        struct LoopModule : MultiTapModule
        {
            const float L1 = std::log2(TAPELOOP_MIN_DELAY_SECONDS);
            const float L2 = std::log2(TAPELOOP_MAX_DELAY_SECONDS);
            bool unhappy = false;
            bool isClockConnected = false;
            bool isMusicalInterval = false;
            TimeMode timeMode = TimeMode::Seconds;
            GateTriggerReceiver reverseReceiver;
            ChannelInfo info[PORT_MAX_CHANNELS];
            PolyControls controls;
            TapInputRouting receivedInputRouting{};
            Smoother clearSmoother;
            ReverseComboSmoother reverseComboSmoother;
            bool polyphonicEnvelopeOutput = true;
            bool flip{};
            bool prevFlip{};
            bool controlsAreDirty{};
            bool controlsAreReady = false;      // prevents accessing invalid memory for uninitialized controls
            PortLabelMode sendReturnPortLabels = PortLabelMode::Stereo;
            SendReturnLocationSmoother sendReturnLocationSmoother;

            explicit LoopModule(std::size_t nParams, std::size_t nOutputPorts)
                : MultiTapModule(nParams, nOutputPorts)
            {
                LoopModule_initialize();
            }

            void LoopModule_initialize()
            {
                reverseComboSmoother.initialize();
                reverseReceiver.initialize();
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    info[c].initialize();
                unhappy = false;
                clearSmoother.initialize();
                sendReturnLocationSmoother.initialize();
                polyphonicEnvelopeOutput = true;
                flip = false;
                controlsAreDirty = true;   // signal we need to update tooltips / hovertext
            }

            void initialize() override
            {
                MultiTapModule::initialize();
                LoopModule_initialize();
            }

            void updateFlipControls()
            {
                const bool needUpdate = controlsAreDirty || (prevFlip != flip);
                if (needUpdate && controlsAreReady)
                {
                    prevFlip = flip;
                    controlsAreDirty = false;

                    const char *name = flip ? "Flip" : "Reverse";
                    getInputInfo(controls.revFlipInputId)->name = name;
                    getParamQuantity(controls.revFlipButtonId)->name = name;
                }
            }

            void toggleFlip()
            {
                auto action = new BoolToggleAction(flip, controlsAreDirty);
                action->redo();
                APP->history->push(action);
            }

            bool isActivelyClocked() const
            {
                // Actively clocked means both are true:
                // 1. The user enabled clocking on this tape loop.
                // 2. There is a cable connected to the CLOCK input port.
                return (timeMode == TimeMode::ClockSync) && isClockConnected;
            }

            void updateReverseState(int inputId, int buttonParamId, int lightId, float sampleRateHz)
            {
                bool active = updateToggleGroup(reverseReceiver, inputId, buttonParamId, lightId);
                if (active)
                    reverseComboSmoother.targetValue = flip ? ReverseFlipMode::Flip : ReverseFlipMode::Reverse;
                else
                    reverseComboSmoother.targetValue = ReverseFlipMode::Forward;
                reverseComboSmoother.process(sampleRateHz);
            }

            json_t* dataToJson() override
            {
                json_t* root = MultiTapModule::dataToJson();
                jsonSetEnum(root, "timeMode", timeMode);
                jsonSetBool(root, "flip", flip);
                jsonSetBool(root, "polyphonicEnvelopeOutput", polyphonicEnvelopeOutput);
                sendReturnLocationSmoother.jsonSave(root);
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                MultiTapModule::dataFromJson(root);
                jsonLoadEnum(root, "timeMode", timeMode);
                jsonLoadBool(root, "flip", flip);
                jsonLoadBool(root, "polyphonicEnvelopeOutput", polyphonicEnvelopeOutput);
                sendReturnLocationSmoother.jsonLoad(root);
                updateFlipControls();
            }

            void updateEnvelope(int outputId, int envGainParamId, float sampleRateHz, const Frame& audio)
            {
                Output& envOutput = outputs.at(outputId);
                if (envOutput.isConnected())
                {
                    const int nc = VcvSafeChannelCount(audio.nchannels);
                    const float gain = FourthPower(params.at(envGainParamId).getValue());

                    if (polyphonicEnvelopeOutput)
                    {
                        envOutput.setChannels(nc);
                        for (int c = 0; c < nc; ++c)
                        {
                            float v = gain * info[c].env.update(audio.sample[c], sampleRateHz);
                            envOutput.setVoltage(v, c);
                        }
                    }
                    else
                    {
                        float sum = 0;
                        for (int c = 0; c < nc; ++c)
                            sum += audio.sample[c];

                        float v = gain * info[0].env.update(sum, sampleRateHz);
                        envOutput.setChannels(1);
                        envOutput.setVoltage(v, 0);
                    }
                }
            }

            ChannelInfo& getChannelInfo(int c)
            {
                return SafeArray(info, PORT_MAX_CHANNELS, c);
            }

            struct TapeLoopResult
            {
                Frame envelopeAudio;
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
                if (inAudio.nchannels==2 && !message.polyphonic)
                    sendReturnPortLabels = PortLabelMode::Stereo;
                else if (inAudio.nchannels==1)
                    sendReturnPortLabels = PortLabelMode::Mono;
                else
                    sendReturnPortLabels = static_cast<PortLabelMode>(inAudio.nchannels);

                clearSmoother.process(sampleRateHz);

                sendReturnLocationSmoother.targetValue =
                    (params.at(controls.sendReturnButtonId).getValue() < 0.5)
                    ? SendReturnLocation::BeforeDelay
                    : SendReturnLocation::AfterDelay;

                sendReturnLocationSmoother.process(sampleRateHz);
                const float srSmooth = sendReturnLocationSmoother.getGain();
                const float smooth = srSmooth * message.routingSmooth;
                receivedInputRouting = message.inputRouting;

                const int nc = inAudio.safeChannelCount();
                Output& sendLeft   = outputs.at(controls.sendLeftOutputId);
                Output& sendRight  = outputs.at(controls.sendRightOutputId);
                Input& returnLeft  = inputs.at(controls.returnLeftInputId);
                Input& returnRight = inputs.at(controls.returnRightInputId);

                TapeLoopResult result;
                result.envelopeAudio.nchannels = nc;
                result.globalAudioOutput.nchannels = nc;
                result.chainAudioOutput.nchannels = nc;
                result.clockVoltage.nchannels = nc;

                float cvDelayTime = 0;
                float vClock = 0;
                float fbk = 0;
                float cvGain = 0;
                int unhappyCount = 0;

                const bool isFirstTap = IsEcho(this);
                const bool isSingleTap = isFirstTap && !IsEchoTap(rightExpander.module);

                // In order to have consistent behavior when there is a single tap,
                // parallel mode and serial mode should mean the same thing.
                // It makes the code simpler to pretend like we are in parallel mode in that case.
                const bool parallelMode = isSingleTap || (message.inputRouting == TapInputRouting::Parallel);
                const bool loopback = isFirstTap && backMessage.valid && !parallelMode;

                for (int c = 0; c < nc; ++c)
                {
                    ChannelInfo& q = getChannelInfo(c);

                    if (controls.clockInputId < 0)
                        vClock = message.clockVoltage.at(c);
                    else
                        vClock = nextChannelInputVoltage(vClock, controls.clockInputId, c);
                    result.clockVoltage.at(c) = vClock;
                    if (q.clockReceiver.updateTrigger(vClock))
                    {
                        // Clock sync. Measure the most recent time interval in samples.
                        float raw = q.samplesSinceClockTrigger / sampleRateHz;
                        q.clockSyncTime = std::clamp(raw, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
                        q.samplesSinceClockTrigger = 0;
                    }
                    else
                    {
                        ++q.samplesSinceClockTrigger;
                    }

                    // Assume the delay time is in seconds.
                    float delayTime = TwoToPower(controlGroupRawCv(c, cvDelayTime, controls.delayTime, L1, L2));

                    // But it might be a dimensionless clock multiplier instead.
                    if (q.clockSyncTime > 0 && isActivelyClocked())
                    {
                        // Are musical intervals enabled? If so, snap to closest fraction.
                        if (isMusicalInterval)
                            delayTime = PickClosestFraction(delayTime).value();
                        delayTime *= q.clockSyncTime;
                    }

                    q.loop.setDelayTime(delayTime, sampleRateHz);
                    q.loop.setInterpolatorKind(message.interpolatorKind);
                    if (clearSmoother.isDelayedActionReady())
                        q.loop.clear();
                    float forward = q.loop.readForward() * clearSmoother.getGain();
                    float reverse = q.loop.readReverse() * clearSmoother.getGain();
                    q.loop.updateReversePlaybackHead();

                    float gain = controlGroupRawCv(c, cvGain, controls.gain, 0, 1);
                    reverseComboSmoother.select(
                        forward,
                        reverse,
                        result.envelopeAudio.at(c),
                        result.chainAudioOutput.at(c)
                    );

                    if (sendReturnLocationSmoother.currentValue == SendReturnLocation::AfterDelay)
                    {
                        writeSample(srSmooth * result.envelopeAudio.at(c), sendLeft, sendRight, c, nc, message.polyphonic);
                        result.envelopeAudio.at(c) = readSample(result.envelopeAudio.at(c), returnLeft, returnRight, c);
                    }
                    result.envelopeAudio.at(c) *= srSmooth;

                    result.globalAudioOutput.at(c) = gain * result.envelopeAudio.at(c);

                    float feedbackSample;
                    if (loopback)
                    {
                        // When we are in serial mode with more than one tap,
                        // feedback comes from the output of the final tap.
                        feedbackSample = backMessage.loopAudio.sample[c];
                    }
                    else if (parallelMode)
                    {
                        // In parallel mode, or in serial mode when there is a single tap,
                        // feedback comes from the same tap's chain output.
                        feedbackSample = result.chainAudioOutput.at(c);
                    }
                    else
                    {
                        // In serial mode, every tap other than the first has NO FEEDBACK.
                        feedbackSample = 0;
                    }

                    if (c < message.feedback.nchannels)
                        fbk = message.feedback.sample[c];

                    const float loopAudio = inAudio.sample[c] + (fbk * feedbackSample);
                    float delayLineInput = loopAudio;
                    if (sendReturnLocationSmoother.currentValue == SendReturnLocation::BeforeDelay)
                    {
                        writeSample(smooth * loopAudio, sendLeft, sendRight, c, nc, message.polyphonic);
                        delayLineInput = readSample(loopAudio, returnLeft, returnRight, c);
                    }
                    delayLineInput = smooth * LinearMix(message.freezeMix, delayLineInput, forward);

                    if (!q.loop.write(delayLineInput, clearSmoother.getGain()))
                        ++unhappyCount;
                }

                result.globalAudioOutput = panFrame(result.globalAudioOutput);
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

            void addPolyphonicEnvelopeMenuItem(ui::Menu* menu)
            {
                menu->addChild(createBoolMenuItem(
                    "Polyphonic envelope output",
                    "",
                    [=]{ return polyphonicEnvelopeOutput; },
                    [=](bool state){ polyphonicEnvelopeOutput = state; }
                ));
            }
        };


        void EnvelopeOutputPort::appendContextMenu(ui::Menu* menu)
        {
            SapphirePort::appendContextMenu(menu);
            auto lmod = dynamic_cast<LoopModule*>(module);
            if (lmod)
            {
                menu->addChild(new MenuSeparator);
                lmod->addPolyphonicEnvelopeMenuItem(menu);
            }
        }


        struct LoopWidget : MultiTapWidget
        {
            const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");
            const float mmShiftFirstTap = (PanelWidth("echo") - PanelWidth("echotap")) / 2;
            const float mmModeButtonRadius = 3.5;
            const float mmChainIndexCenterY = 4.5;
            bool hilightInputRoutingButton = false;
            bool hilightRevFlipButton = false;
            SvgOverlay* revLabel = nullptr;
            SvgOverlay* revSelLabel = nullptr;
            SvgOverlay* flpLabel = nullptr;
            SvgOverlay* flpSelLabel = nullptr;
            Vec flpRevLabelPos;
            float dxFlipRev{};
            float dyFlipRev{};
            SapphireTooltip* routingTooltip = nullptr;
            SapphireTooltip* revFlipTooltip = nullptr;

            explicit LoopWidget(
                const std::string& moduleCode,
                const std::string& panelSvgFileName,
                const std::string& revSvgFileName,
                const std::string& revSelSvgFileName,
                const std::string& flpSvgFileName,
                const std::string& flpSelSvgFileName
            )
                : MultiTapWidget(moduleCode, panelSvgFileName)
            {
                revLabel = SvgOverlay::Load(revSvgFileName);
                addChild(revLabel);
                revLabel->hide();

                revSelLabel = SvgOverlay::Load(revSelSvgFileName);
                addChild(revSelLabel);
                revSelLabel->hide();

                flpLabel = SvgOverlay::Load(flpSvgFileName);
                addChild(flpLabel);
                flpLabel->hide();

                flpSelLabel = SvgOverlay::Load(flpSelSvgFileName);
                addChild(flpSelLabel);
                flpSelLabel->hide();

                ComponentLocation centerLoc = FindComponent(modcode, "label_flp_rev");
                flpRevLabelPos = Vec(mm2px(centerLoc.cx), mm2px(centerLoc.cy));

                ComponentLocation inputLoc  = FindComponent(modcode, "reverse_input");
                ComponentLocation buttonLoc = FindComponent(modcode, "reverse_button");
                const float dxCushion = 8.0;
                dxFlipRev = mm2px(buttonLoc.cx - inputLoc.cx - dxCushion) / 2;
                dyFlipRev = mm2px(2.5);
            }

            virtual ~LoopWidget()
            {
                destroyTooltip(routingTooltip);
                destroyTooltip(revFlipTooltip);
            }

            void addSendReturnButton(int buttonParamId)
            {
                auto button = createParamCentered<SendReturnButton>(Vec{}, module, buttonParamId);
                button->loopWidget = this;
                addSapphireParam(button, "sendreturn_button");
            }

            void addExpanderInsertButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createParamCentered<InsertButton>(Vec{}, loopModule, paramId);
                button->loopWidget = this;
                addSapphireParam(button, "insert_button");
            }

            void addReverseToggleGroup(int inputId, int buttonParamId, int buttonLightId)
            {
                addToggleGroup(
                    "reverse",
                    inputId,
                    buttonParamId,
                    buttonLightId,
                    '\0',
                    7.0,
                    SCHEME_PURPLE
                );
            }

            void addExpanderRemoveButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createParamCentered<RemoveButton>(Vec{}, loopModule, paramId);
                button->loopWidget = this;
                addSapphireParam(button, "remove_button");
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

            void removeExpander()
            {
                if (module == nullptr)
                    return;

                // Hand responsibility for moving the rest of the chain to the next module in the chain.
                auto nextModule = dynamic_cast<MultiTapModule*>(module->rightExpander.module);
                if (nextModule)
                    nextModule->beginMoveChain(box.pos.x);

                // *** DANGER DANGER DANGER ***
                // The following code deletes `this`.
                // Can't do anything more to this object!
                removeAction();
            }

            virtual bool isConnectedOnLeft() const = 0;

            bool isConnectedOnRight() const
            {
                return module && IsEchoReceiver(module->rightExpander.module);
            }

            void step() override
            {
                MultiTapWidget::step();
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    lmod->hideLeftBorder  = isConnectedOnLeft();
                    lmod->hideRightBorder = isConnectedOnRight();
                    flpLabel->setVisible(lmod->flip && !hilightRevFlipButton);
                    flpSelLabel->setVisible(lmod->flip && hilightRevFlipButton);
                    revLabel->setVisible(!lmod->flip && !hilightRevFlipButton);
                    revSelLabel->setVisible(!lmod->flip && hilightRevFlipButton);
                    lmod->updateFlipControls();
                }
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

            bool isInsideFlipRevButton(Vec pos) const
            {
                const float dx = pos.x - flpRevLabelPos.x;
                const float dy = pos.y - flpRevLabelPos.y;
                return (std::abs(dx) < dxFlipRev) && (std::abs(dy) < dyFlipRev);
            }

            void onMousePress(const ButtonEvent& e)
            {
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    if (offerRoutingModeChange() && isInsideInputRoutingButton(e.pos))
                        lmod->bumpTapInputRouting();

                    if (isInsideFlipRevButton(e.pos))
                        lmod->toggleFlip();
                }
            }

            void onMouseRelease(const ButtonEvent& e)
            {
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
                MultiTapWidget::onButton(e);
            }

            void updateRoutingButton(bool state)
            {
                updateTooltip(hilightInputRoutingButton, state, routingTooltip, "Toggle serial/parallel");
            }

            void updateFlipRevButton(bool state)
            {
                updateTooltip(hilightRevFlipButton, state, revFlipTooltip, "Toggle reverse/flip");
            }

            void onHover(const HoverEvent& e) override
            {
                updateRoutingButton(isInsideInputRoutingButton(e.pos));
                updateFlipRevButton(isInsideFlipRevButton(e.pos));
                MultiTapWidget::onHover(e);
            }

            void onLeave(const LeaveEvent& e) override
            {
                updateRoutingButton(false);
                updateFlipRevButton(false);
                MultiTapWidget::onLeave(e);
            }

            bool offerRoutingModeChange() const
            {
                // Do not show S/P when the chain consists of a single Echo module.
                // In this case, Serial and Parallel mean the exact same thing,
                // so it would be confusing to offer a choice that has no effect.
                return IsEcho(module) && IsEchoTap(module->rightExpander.module);
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

                char text[20];

                nvgFontSize(vg, 18);
                nvgFontFaceId(vg, font->handle);
                nvgFillColor(vg, textColor);

                const bool isDisconnectedEcho = IsEcho(module) && !IsEchoReceiver(module->rightExpander.module);
                if ((chainIndex > 0) && !isDisconnectedEcho)
                {
                    snprintf(text, sizeof(text), "%d", chainIndex);
                    Vec center = getChainIndexCenterPos();
                    drawCenteredText(vg, center.x, center.y, text);
                }

                if (offerRoutingModeChange())
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
                MultiTapWidget::draw(args);

                auto lmod = dynamic_cast<const LoopModule*>(module);
                if (lmod)
                {
                    if (!lmod->neonMode)
                        drawChainIndex(args.vg, lmod->chainIndex, lmod->receivedInputRouting, nvgRGB(0x66, 0x06, 0x5c));

                    ComponentLocation L = FindComponent(modcode, "sendreturn_label_left");
                    ComponentLocation R = FindComponent(modcode, "sendreturn_label_right");
                    drawAudioPortLabels(args.vg, lmod->sendReturnPortLabels, L.cx, L.cy, R.cy);
                }
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                MultiTapWidget::drawLayer(args, layer);
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
                MultiTapWidget::appendContextMenu(menu);

                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    menu->addChild(lmod->createToggleAllSensitivityMenuItem());
                    lmod->addPolyphonicEnvelopeMenuItem(menu);
                }
            }

            TimeKnob* addTimeControlGroup(int paramId, int attenId, int cvInputId)
            {
                TimeKnob* tk = addSapphireFlatControlGroup<TimeKnob>("time", paramId, attenId, cvInputId);
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    tk->mode = &(lmod->timeMode);
                    tk->isClockConnected = &(lmod->isClockConnected);
                    tk->isMusicalInterval = &(lmod->isMusicalInterval);
                }
                return tk;
            }

            EnvelopeOutputPort* addEnvelopeOutput(int outputId)
            {
                return addSapphireOutput<EnvelopeOutputPort>(outputId, "env_output");
            }

            InitTapButton* addInitTapButton(int buttonParamId)
            {
                auto button = createParamCentered<InitTapButton>(Vec{}, module, buttonParamId);
                button->loopWidget = this;
                addSapphireParam(button, "init_tap_button");
                return button;
            }
        };


        void InsertButton::onButton(const event::Button& e)
        {
            insert_button_base_t::onButton(e);
            if (loopWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    loopWidget->insertExpander();
            }
        }


        void RemoveButton::onButton(const event::Button& e)
        {
            remove_button_base_t::onButton(e);
            if (loopWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    loopWidget->removeExpander();
            }
        }


        static const std::vector<Fraction> MusicalFractions =
        {
            { 1,  8, "1/32 note"   },
            { 1,  6, "1/16 trip"   },
            { 3, 16, "1/32 dot"    },
            { 1,  4, "1/16 note"   },
            { 1,  3, "1/8 trip"    },
            { 3,  8, "1/16 dot"    },
            { 1,  2, "1/8 note"    },
            { 2,  3, "1/4 trip"    },
            { 3,  4, "1/8 dot"     },
            { 1,  1, "1/4 note"    },
            { 4,  3, "1/2 trip"    },
            { 3,  2, "1/4 dot"     },
            { 2,  1, "1/2 note"    },
            { 3,  1, "1/2 dot"     },
            { 4,  1, "whole note"  },
            { 6,  1, "whole dot"   },
            { 8,  1, "double note" },
        };


        const Fraction& PickClosestFraction(float ratio)
        {
            if (!std::isfinite(ratio) || ratio > 8.0)
                return MusicalFractions.back();

            if (ratio <= 0.0)
                return MusicalFractions.front();

            // It makes things simple if we can snap to closest integers.
            // We have 16 in a denominator and 3. Multiply 3*16 = 48.
            const Fraction* best = nullptr;
            float bestScore = 0;

            for (const Fraction& frac : MusicalFractions)
            {
                float diff = std::abs(ratio - frac.value());
                if (best==nullptr || diff<bestScore)
                {
                    bestScore = diff;
                    best = &frac;
                }
            }

            if (best == nullptr)
                throw std::logic_error("How could I not find a fraction?");

            return *best;
        }


        std::string TimeKnobParamQuantity::getString()
        {
            float value = rack::normalizeZero(getDisplayValue());
            std::string text = rack::string::f("%.5g", value);
            auto lmod = dynamic_cast<const LoopModule*>(module);
            if (lmod)
            {
                if (lmod->isActivelyClocked())
                {
                    if (lmod->isMusicalInterval)
                    {
                        const Fraction& frac = PickClosestFraction(value);
                        return "CLOCK x " + frac.format();
                    }
                    return "CLOCK x " + text;
                }
            }
            return text + " sec";
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
                INTERVAL_BUTTON_PARAM,
                GAIN_PARAM,
                GAIN_ATTEN,
                REVERSE_BUTTON_PARAM,
                FREEZE_BUTTON_PARAM,
                CLEAR_BUTTON_PARAM,
                ENV_GAIN_PARAM,
                CLOCK_BUTTON_PARAM,
                SEND_RETURN_BUTTON_PARAM,
                INIT_CHAIN_BUTTON_PARAM,
                INIT_TAP_BUTTON_PARAM,
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

            class RoutingSmoother : public EnumSmoother<TapInputRouting>
            {
            public:
                explicit RoutingSmoother()
                    : EnumSmoother(TapInputRouting::Default, "tapInputRouting")
                    {}
            };

            struct EchoModule : LoopModule
            {
                // Global controls
                GateTriggerReceiver freezeReceiver;
                AnimatedTriggerReceiver clearReceiver;
                RoutingSmoother routingSmoother;
                InterpolatorKind interpolatorKind{};
                Crossfader freezeFader;
                PortLabelMode inputLabels{};

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
                    configToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, "Reverse", "Reverse gate");
                    configToggleGroup(FREEZE_INPUT, FREEZE_BUTTON_PARAM, "Freeze", "Freeze gate");
                    configToggleGroup(CLEAR_INPUT, CLEAR_BUTTON_PARAM, "Clear", "Clear trigger");
                    configInput(CLOCK_INPUT, "Clock");
                    configButton(CLOCK_BUTTON_PARAM, "Toggle all clock sync");
                    configButton(INTERVAL_BUTTON_PARAM, "Snap to musical intervals");
                    configButton(SEND_RETURN_BUTTON_PARAM, "Toggle send/return location");
                    configButton(INIT_CHAIN_BUTTON_PARAM, "Initialize entire chain");
                    configButton(INIT_TAP_BUTTON_PARAM, "Initialize this tap only");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    addDcRejectQuantity(DC_REJECT_PARAM, 20);
                    EchoModule_initialize();
                    controlsAreReady = true;
                }

                void EchoModule_initialize()
                {
                    routingSmoother.initialize();
                    interpolatorKind = InterpolatorKind::Linear;
                    freezeReceiver.initialize();
                    clearReceiver.initialize();
                    dcRejectQuantity->initialize();
                    params.at(REVERSE_BUTTON_PARAM).setValue(0);
                    params.at(FREEZE_BUTTON_PARAM).setValue(0);
                    params.at(CLEAR_BUTTON_PARAM).setValue(0);
                    params.at(INIT_CHAIN_BUTTON_PARAM).setValue(0);
                    params.at(INIT_TAP_BUTTON_PARAM).setValue(0);
                    freezeFader.snapToFront();      // front=false=0, back=true=1
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
                    controls.revFlipButtonId = REVERSE_BUTTON_PARAM;
                    controls.revFlipInputId = REVERSE_INPUT;
                    controls.sendReturnButtonId = SEND_RETURN_BUTTON_PARAM;
                }

                void process(const ProcessArgs& args) override
                {
                    Message outMessage;
                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();
                    isMusicalInterval = outMessage.musicalInterval = (params.at(INTERVAL_BUTTON_PARAM).getValue() > 0.5);
                    outMessage.routingSmooth = routingSmoother.process(args.sampleRate);
                    outMessage.inputRouting = routingSmoother.currentValue;
                    outMessage.polyphonic = isPolyphonic(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);
                    outMessage.freezeMix = updateFreezeState(args.sampleRate);
                    updateReverseState(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, args.sampleRate);
                    outMessage.clear = updateClearState(args.sampleRate);
                    outMessage.chainIndex = 2;
                    outMessage.originalAudio = readOriginalAudio(args.sampleRate, inputLabels);
                    outMessage.feedback = getFeedbackPoly();
                    isClockConnected = outMessage.isClockConnected = inputs.at(CLOCK_INPUT).isConnected();
                    outMessage.interpolatorKind = interpolatorKind;
                    TapeLoopResult result = updateTapeLoops(outMessage.originalAudio, args.sampleRate, outMessage, inBackMessage);
                    outMessage.chainAudio = result.chainAudioOutput;
                    outMessage.summedAudio = result.globalAudioOutput;
                    outMessage.clockVoltage = result.clockVoltage;
                    outMessage.neonMode = neonMode;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, result.envelopeAudio);
                    sendMessage(outMessage);
                }

                Frame readOriginalAudio(float sampleRateHz, PortLabelMode& mode)
                {
                    Frame audio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, mode);
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
                    routingSmoother.jsonSave(root);
                    jsonSetEnum(root, "interpolatorKind", interpolatorKind);
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    LoopModule::dataFromJson(root);
                    routingSmoother.jsonLoad(root);
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

                float updateFreezeState(float sampleRateHz)
                {
                    const bool freezeGate = updateToggleGroup(
                        freezeReceiver,
                        FREEZE_INPUT,
                        FREEZE_BUTTON_PARAM,
                        FREEZE_BUTTON_LIGHT
                    );
                    freezeFader.beginFade(freezeGate);
                    return freezeFader.process(sampleRateHz, 0, 1);
                }

                bool updateClearState(float sampleRateHz)
                {
                    const bool clearRequested = updateTriggerGroup(
                        sampleRateHz,
                        clearReceiver,
                        CLEAR_INPUT,
                        CLEAR_BUTTON_PARAM,
                        CLEAR_BUTTON_LIGHT
                    );

                    if (clearRequested)
                        clearSmoother.begin();

                    return clearRequested;
                }

                void bumpTapInputRouting() override
                {
                    routingSmoother.beginBumpEnum();
                    APP->history->push(new BumpEnumAction(routingSmoother));
                }
            };

            struct EchoWidget : LoopWidget
            {
                EchoModule* echoModule{};
                int creationCountdown = 8;

                explicit EchoWidget(EchoModule* module)
                    : LoopWidget(
                        "echo",
                        asset::plugin(pluginInstance, "res/echo.svg"),
                        asset::plugin(pluginInstance, "res/echo_rev.svg"),
                        asset::plugin(pluginInstance, "res/echo_rev_sel.svg"),
                        asset::plugin(pluginInstance, "res/echo_flp.svg"),
                        asset::plugin(pluginInstance, "res/echo_flp_sel.svg")
                    )
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
                    addIntervalButton();
                    addInitChainButton();

                    // Per-tap controls/ports
                    addSendReturnButton(SEND_RETURN_BUTTON_PARAM);
                    addStereoOutputPorts(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    addStereoInputPorts(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    addTimeControlGroup(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    addReverseToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    addEnvelopeOutput(ENV_OUTPUT);
                    addSmallKnob(ENV_GAIN_PARAM, "env_gain_knob");
                    addInitTapButton(INIT_TAP_BUTTON_PARAM);
                }

                void addClockButton()
                {
                    auto button = createParamCentered<ClockButton>(Vec{}, echoModule, CLOCK_BUTTON_PARAM);
                    button->echoWidget = this;
                    addSapphireParam(button, "clock_button");
                }

                void addIntervalButton()
                {
                    auto button = createParamCentered<IntervalButton>(Vec{}, echoModule, INTERVAL_BUTTON_PARAM);
                    addSapphireParam(button, "interval_button");
                }

                void addInitChainButton()
                {
                    auto button = createParamCentered<InitChainButton>(Vec{}, echoModule, INIT_CHAIN_BUTTON_PARAM);
                    button->echoWidget = this;
                    addSapphireParam(button, "init_chain_button");
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
                    ComponentLocation syncButton = FindComponent(modcode, "clock_button");
                    ComponentLocation intervalButton = FindComponent(modcode, "interval_button");
                    static constexpr float MULTITAP_CLOCK_BUTTON_RADIUS = 1.5;
                    float bx = mm2px(syncButton.cx - MULTITAP_CLOCK_BUTTON_RADIUS);
                    float by = mm2px(syncButton.cy);

                    float cx = mm2px(intervalButton.cx - MULTITAP_CLOCK_BUTTON_RADIUS);
                    float cy = mm2px(intervalButton.cy);

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

                    // Draw connector line from right edge of rectangle
                    // to the edge of the clock-sync toggle button.
                    nvgMoveTo(vg, x2, by);
                    nvgLineTo(vg, bx, by);

                    // Draw another connector line to the interval button.
                    nvgMoveTo(vg, x2, cy);
                    nvgLineTo(vg, cx, cy);

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
                        drawClockSyncSymbol(args.vg, SCHEME_CYAN, 1.25);
                }

                void draw(const DrawArgs& args) override
                {
                    LoopWidget::draw(args);

                    PortLabelMode label = echoModule ? echoModule->inputLabels : PortLabelMode::Stereo;
                    ComponentLocation L = FindComponent(modcode, "input_label_left");
                    ComponentLocation R = FindComponent(modcode, "input_label_right");
                    drawAudioPortLabels(args.vg, label, L.cx, L.cy, R.cy);

                    if (!isClockPortConnected())
                        drawClockSyncSymbol(args.vg, SCHEME_BLACK, 1.25);
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

                        menu->addChild(createMenuItem(
                            "Initialize Echo expander chain",
                            "",
                            [=]{ initializeExpanderChain(); }
                        ));

                        menu->addChild(createEnumMenuItem(
                            "Signal routing",
                            {
                                "Parallel",
                                "Serial"
                            },
                            echoModule->routingSmoother.targetValue
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
                            "Toggle all clock sync",
                            "",
                            [=]{ toggleAllClockSync(); }
                        ));

                        menu->addChild(createMenuItem(
                            "Toggle polyphonic/mono on all envelope followers",
                            "",
                            [=]{ toggleAllPolyphonicEnvelope(); }
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

                int tallyTaps(std::function<bool(const LoopModule*)> predicate) const
                {
                    int count = 0;
                    if (echoModule)
                    {
                        if (predicate(echoModule))
                            ++count;

                        Module* module = echoModule->rightExpander.module;
                        while (IsEchoTap(module))
                        {
                            auto lmod = dynamic_cast<const LoopModule*>(module);
                            if (lmod && predicate(lmod))
                                ++count;

                            module = module->rightExpander.module;
                        }
                    }
                    return count;
                }

                void visitTaps(std::function<void(LoopModule* lmod)> visit)
                {
                    if (echoModule)
                    {
                        visit(echoModule);
                        Module* module = echoModule->rightExpander.module;
                        while (IsEchoTap(module))
                        {
                            auto lmod = dynamic_cast<LoopModule*>(module);
                            if (lmod)
                                visit(lmod);

                            module = module->rightExpander.module;
                        }
                    }
                }

                void toggleAllPolyphonicEnvelope()
                {
                    const int countPoly = tallyTaps(
                        [](const LoopModule *lmod)
                        {
                            return lmod->polyphonicEnvelopeOutput;
                        }
                    );

                    const int countMono = tallyTaps(
                        [](const LoopModule *lmod)
                        {
                            return !lmod->polyphonicEnvelopeOutput;
                        }
                    );

                    const bool newPoly = (countPoly < countMono);
                    visitTaps(
                        [=](LoopModule *lmod)
                        {
                            lmod->polyphonicEnvelopeOutput = newPoly;
                        }
                    );
                }

                void toggleAllClockSync()
                {
                    const int totalCount = tallyTaps(
                        [](const LoopModule *lmod)
                        {
                            return true;
                        }
                    );

                    const int clockCount = tallyTaps(
                        [](const LoopModule *lmod)
                        {
                            return lmod->timeMode == TimeMode::ClockSync;
                        }
                    );

                    const TimeMode timeMode = (
                        (2*clockCount > totalCount) ?
                        TimeMode::Seconds :
                        TimeMode::ClockSync
                    );

                    visitTaps([=](LoopModule *lmod)
                    {
                        lmod->timeMode = timeMode;
                    });
                }
            };
        }

        void ClockButton::onButton(const ButtonEvent& e)
        {
            if (echoWidget)
            {
                if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS)
                {
                    echoWidget->toggleAllClockSync();
                    stopwatch.restart();
                }
            }
            clock_button_base_t::onButton(e);
        }

        void ClockButton::step()
        {
            clock_button_base_t::step();
            const float blinkTime = 0.02;
            if (stopwatch.elapsedSeconds() >= blinkTime)
            {
                stopwatch.reset();
                ParamQuantity *quantity = getParamQuantity();
                if (quantity && quantity->getValue() > 0)
                    quantity->setValue(0);
            }
        }

        void InitChainButton::onButton(const ButtonEvent& e)
        {
            if (echoWidget)
            {
                if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS)
                    echoWidget->initializeExpanderChain();
            }
            init_chain_button_base_t::onButton(e);
        }

        void InitTapButton::onButton(const ButtonEvent& e)
        {
            if (loopWidget)
            {
                if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS)
                    loopWidget->resetAction();
            }
            init_chain_button_base_t::onButton(e);
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
                SEND_RETURN_BUTTON_PARAM,
                REMOVE_BUTTON_PARAM,
                GAIN_PARAM,
                GAIN_ATTEN,
                REVERSE_BUTTON_PARAM,
                ENV_GAIN_PARAM,
                INIT_CHAIN_BUTTON_PARAM,
                INIT_TAP_BUTTON_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                TIME_CV_INPUT,
                PAN_CV_INPUT,
                _OBSOLETE_INPUT,
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
                _OBSOLETE_LIGHT,
                REMOVE_BUTTON_LIGHT,
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
                    configButton(REMOVE_BUTTON_PARAM, "Remove tap");
                    configButton(SEND_RETURN_BUTTON_PARAM, "Toggle send/return location");
                    configTimeControls(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    configPanControls(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    configToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, "Reverse", "Reverse gate");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    configButton(INIT_CHAIN_BUTTON_PARAM, "Initialize entire chain");
                    configButton(INIT_TAP_BUTTON_PARAM, "Initialize this tap only");
                    EchoTapModule_initialize();
                    controlsAreReady = true;
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
                    controls.revFlipButtonId = REVERSE_BUTTON_PARAM;
                    controls.revFlipInputId = REVERSE_INPUT;
                    controls.sendReturnButtonId = SEND_RETURN_BUTTON_PARAM;
                }

                void process(const ProcessArgs& args) override
                {
                    const Message inMessage = receiveMessageOrDefault();
                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();

                    // Copy input to output by default, then patch whatever is different.
                    Message outMessage = inMessage;
                    isMusicalInterval = inMessage.musicalInterval;
                    chainIndex = inMessage.chainIndex;
                    isClockConnected = inMessage.isClockConnected;
                    includeNeonModeMenuItem = !inMessage.valid;

                    if (inMessage.valid)
                        neonMode = inMessage.neonMode;

                    updateReverseState(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, args.sampleRate);

                    if (inMessage.clear)
                        clearSmoother.begin();

                    outMessage.chainIndex = (chainIndex < 0) ? -1 : (1 + chainIndex);

                    const Frame& tapInputAudio =
                        (inMessage.inputRouting == TapInputRouting::Parallel) ?
                        inMessage.originalAudio :
                        inMessage.chainAudio;

                    TapeLoopResult result = updateTapeLoops(tapInputAudio, args.sampleRate, outMessage, inBackMessage);
                    outMessage.chainAudio = result.chainAudioOutput;
                    outMessage.summedAudio += result.globalAudioOutput;
                    outMessage.clockVoltage = result.clockVoltage;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, result.envelopeAudio);
                    sendMessage(outMessage);

                    BackwardMessage outBackMessage;
                    if (inBackMessage.valid)
                        outBackMessage = inBackMessage;
                    else
                        outBackMessage.loopAudio = result.chainAudioOutput;
                    sendBackwardMessage(outBackMessage);
                }
            };


            struct EchoTapWidget : LoopWidget
            {
                EchoTapModule* echoTapModule{};

                explicit EchoTapWidget(EchoTapModule* module)
                    : LoopWidget(
                        "echotap",
                        asset::plugin(pluginInstance, "res/echotap.svg"),
                        asset::plugin(pluginInstance, "res/echotap_rev.svg"),
                        asset::plugin(pluginInstance, "res/echotap_rev_sel.svg"),
                        asset::plugin(pluginInstance, "res/echotap_flp.svg"),
                        asset::plugin(pluginInstance, "res/echotap_flp_sel.svg")
                    )
                    , echoTapModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addExpanderRemoveButton(module, REMOVE_BUTTON_PARAM, REMOVE_BUTTON_LIGHT);
                    addSendReturnButton(SEND_RETURN_BUTTON_PARAM);
                    addStereoOutputPorts(SEND_LEFT_OUTPUT, SEND_RIGHT_OUTPUT, "send");
                    addStereoInputPorts(RETURN_LEFT_INPUT, RETURN_RIGHT_INPUT, "return");
                    addTimeControlGroup(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    addReverseToggleGroup(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT);
                    addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    addEnvelopeOutput(ENV_OUTPUT);
                    addSmallKnob(ENV_GAIN_PARAM, "env_gain_knob");
                    addInitTapButton(INIT_TAP_BUTTON_PARAM);
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
                PortLabelMode outputLabels = PortLabelMode::Stereo;

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

                    includeNeonModeMenuItem = !message.valid;
                    if (message.valid)
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

            struct EchoOutWidget : MultiTapWidget
            {
                EchoOutModule* echoOutModule{};

                explicit EchoOutWidget(EchoOutModule* module)
                    : MultiTapWidget("echoout", asset::plugin(pluginInstance, "res/echoout.svg"))
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
                    MultiTapWidget::step();
                    SapphireModule* smod = getSapphireModule();
                    if (smod)
                        smod->hideLeftBorder = isConnectedOnLeft();
                }

                void draw(const DrawArgs& args) override
                {
                    MultiTapWidget::draw(args);
                    PortLabelMode label = echoOutModule ? echoOutModule->outputLabels : PortLabelMode::Stereo;
                    ComponentLocation L = FindComponent(modcode, "output_label_left");
                    ComponentLocation R = FindComponent(modcode, "output_label_right");
                    drawAudioPortLabels(args.vg, label, L.cx, L.cy, R.cy);
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
