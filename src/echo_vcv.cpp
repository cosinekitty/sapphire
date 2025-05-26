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
            LEN,

            Default = Seconds,
        };


        struct TimeKnobInfo
        {
            TimeMode timeMode = TimeMode::Default;
            bool isMusicalInterval = false;
            bool isClockConnected = false;
            int deadClockCountdown = 0;
            bool isReceivingClockTriggers = false;

            void initialize()
            {
                timeMode = TimeMode::Default;
                isClockConnected = false;
                isMusicalInterval = false;
                deadClockCountdown = 0;
                isReceivingClockTriggers = false;
            }

            NVGcolor color() const
            {
                if (deadClockCountdown > 0)
                    return SCHEME_RED;

                if (isReceivingClockTriggers)
                    return SCHEME_CYAN;

                return nvgRGB(0xb0, 0xb0, 0x90);
            }
        };


        using time_knob_base_t = RoundSmallBlackKnob;
        struct TimeKnob : time_knob_base_t
        {
            TimeKnobInfo* info = nullptr;

            TimeMode getMode() const
            {
                return info ? info->timeMode : TimeMode::Default;
            }

            bool isClockCableConnected() const
            {
                return info && info->isClockConnected;
            }

            bool snapMusicalIntervals() const
            {
                return info && info->isMusicalInterval;
            }

            bool isClockWarningEnabled() const
            {
                return info && (info->deadClockCountdown > 0);
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

                const NVGcolor color = info ? info->color() : nvgRGB(0x90, 0x90, 0x90);

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
                if (info)
                {
                    menu->addChild(new MenuSeparator);
                    menu->addChild(createEnumMenuItem(
                        "Time mode",
                        {
                            "Seconds",
                            "Clock sync"
                        },
                        info->timeMode
                    ));
                }
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


        using input_mode_button_base_t = app::SvgSwitch;
        struct InputModeButton : input_mode_button_base_t
        {
            Echo::EchoWidget* echoWidget{};

            explicit InputModeButton()
            {
                momentary = false;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/clock_button_1.svg")));
            }
        };


        struct MuteButton : app::SvgSwitch
        {
            LoopWidget* loopWidget{};

            explicit MuteButton()
            {
                momentary = false;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/mute_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/mute_button_1.svg")));
            }
        };


        struct SoloButton : app::SvgSwitch
        {
            LoopWidget* loopWidget{};

            explicit SoloButton()
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


        constexpr unsigned GraphSliceCount = 400;
        constexpr float HorMarginPx = 1.0;
        constexpr float VerMarginPx = 1.0;
        constexpr float GraphVoltageLimit = 10;

        constexpr unsigned SliceInc(unsigned sliceIndex)
        {
            return (sliceIndex + 1) % (GraphSliceCount + 1);
        }


        struct GraphSlice
        {
            // Polyphonic sums-of-squares to measure mean power density in each slice.
            unsigned nsamples = 0;
            Frame sumOutput;
            Frame sumInput;

            void initialize()
            {
                nsamples = 0;
                sumInput.initialize();
                sumOutput.initialize();
            }
        };


        struct GraphWidget : OpaqueWidget
        {
            static constexpr float ThinLineFraction = 0.01;
            LoopModule* loopModule = nullptr;
            std::vector<GraphSlice> sliceArray;      // each slice is an "envelope" frame that summarizes the energy in that slice
            unsigned sliceIndex = 0;
            double timeAccum = 0;
            double delayTimeSeconds = 0;
            int currentNumChannels = 0;
            float zoom = 5;     // FIXFIXFIX: allow zooming?

            explicit GraphWidget(LoopModule* _loopModule, float x1, float y1, float x2, float y2)
                : loopModule(_loopModule)
            {
                sliceArray.resize(GraphSliceCount+1);
                box.pos.x  = mm2px(x1);
                box.pos.y  = mm2px(y1);
                box.size.x = mm2px(x2-x1);
                box.size.y = mm2px(y2-y1);
                initialize();
            }

            virtual ~GraphWidget();

            void initialize()
            {
                for (GraphSlice& slice : sliceArray)
                    slice.initialize();

                timeAccum = 0;
                sliceIndex = 0;
                currentNumChannels = 0;
            }

            void drawLayer(const DrawArgs& args, int layer) override;
            void drawPowerFrame(NVGcontext* vg, const Frame& power, int sliceIndex, NVGcolor color, float left, float right);

            void draw(const DrawArgs& args) override
            {
                math::Rect r = box.zeroPos();
                nvgBeginPath(args.vg);
                nvgRect(args.vg, RECT_ARGS(r));
                nvgFillColor(args.vg, SCHEME_BLACK);
                nvgFill(args.vg);
                OpaqueWidget::draw(args);
            }

            Vec position(
                unsigned s,  // slice vertical position: 0..GraphSliceCount
                int c,       // channel: 0..(currentNumChannels-1)
                float p      // relative position in column: (-1)..(+1)
            ) const
            {
                if (currentNumChannels<=0 || currentNumChannels>PORT_MAX_CHANNELS)
                    return Vec{0, 0};

                float pixelsPerChannel = box.size.x / currentNumChannels;
                float xmid = (c + 0.5f)*pixelsPerChannel;
                float pixelsPerUnit = std::max<float>(2, pixelsPerChannel/2 - HorMarginPx/(1+currentNumChannels));
                float x = xmid + (pixelsPerUnit * std::clamp<float>(p, -1, +1));

                float yRatio = static_cast<float>(s) / static_cast<float>(GraphSliceCount);
                float y = VerMarginPx + yRatio*(box.size.y - 2*VerMarginPx);

                return Vec{x, y};
            }

            void setDelayTime(float _delayTimeSeconds)
            {
                delayTimeSeconds = _delayTimeSeconds;
            }

            void process(const Frame& inputAudio, const Frame& outputAudio, double sampleRateHz)
            {
                // No graphics until we know the delay time!
                if (!std::isfinite(delayTimeSeconds) || delayTimeSeconds<=0)
                    return;

                currentNumChannels = std::max(
                    inputAudio.safeChannelCount(),
                    outputAudio.safeChannelCount()
                );

                // To summarize "power" in a slice, we use RMS amplitude:
                // root-mean-square = sqrt((s0^2 + s1^2 + ...)/n)
                // In each slice we store the running sum of squares.
                GraphSlice& slice = sliceArray.at(sliceIndex);
                ++slice.nsamples;
                slice.sumInput.nchannels = currentNumChannels;
                slice.sumOutput.nchannels = currentNumChannels;
                for (int c = 0; c < currentNumChannels; ++c)
                {
                    slice.sumInput.sample[c]  += Square(inputAudio.sample[c]);
                    slice.sumOutput.sample[c] += Square(outputAudio.sample[c]);
                }

                timeAccum += 1/sampleRateHz;
                const double timeLimit = delayTimeSeconds / GraphSliceCount;
                if (timeAccum >= timeLimit)
                {
                    // We have finished another slice!
                    // Do the final RMS = sqrt(sum/nsamples) calculation for both input and output.
                    timeAccum = 0;
                    slice.sumInput.rootMeanSquare(slice.nsamples);
                    slice.sumOutput.rootMeanSquare(slice.nsamples);

                    // Start fresh on the next slice.
                    // Erase the old power value and start with a new sum-of-squares.
                    sliceIndex = SliceInc(sliceIndex);
                    sliceArray.at(sliceIndex).initialize();
                }
            }
        };


        struct LoopModule : MultiTapModule
        {
            const float L1 = std::log2(TAPELOOP_MIN_DELAY_SECONDS);
            const float L2 = std::log2(TAPELOOP_MAX_DELAY_SECONDS);
            bool recordingLevelOverflow = false;
            TimeKnob* timeKnob = nullptr;
            TimeKnobInfo timeKnobInfo;
            ToggleGroup reverseToggleGroup;
            ChannelInfo info[PORT_MAX_CHANNELS];
            PolyControls controls;
            TapInputRouting receivedInputRouting{};
            Smoother clearSmoother;
            ReverseComboSmoother reverseComboSmoother;
            bool polyphonicEnvelopeOutput{};
            bool flip{};
            bool flipControlsAreDirty{};
            bool duck{};
            bool duckControlsAreDirty{};
            Crossfader envDuckFader;
            bool prevFlip{};
            bool controlsAreReady = false;      // prevents accessing invalid memory for uninitialized controls
            PortLabelMode sendReturnPortLabels = PortLabelMode::Stereo;
            SendReturnLocationSmoother sendReturnLocationSmoother;
            bool sendReturnControlsAreDirty{};
            Crossfader muteFader;
            Crossfader soloFader;
            GraphWidget* graph = nullptr;
            int totalSoloCount = 0;
            ClockSignalFormat clockSignalFormat = ClockSignalFormat::Default;

            explicit LoopModule(std::size_t nParams, std::size_t nOutputPorts)
                : MultiTapModule(nParams, nOutputPorts)
            {
                LoopModule_initialize();
            }

            virtual ~LoopModule()
            {
                // Before this object is freed, sever links from widgets into its memory.
                // Otherwise, the widgets can access freed memory, causing crashes/hangs.

                if (graph)
                    graph->loopModule = nullptr;

                if (timeKnob)
                    timeKnob->info = nullptr;
            }

            void LoopModule_initialize()
            {
                timeKnobInfo.initialize();
                reverseComboSmoother.initialize();
                reverseToggleGroup.initialize();
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    info[c].initialize();
                recordingLevelOverflow = false;
                clearSmoother.initialize();
                sendReturnLocationSmoother.initialize();
                polyphonicEnvelopeOutput = false;
                flip = false;
                duck = false;
                flipControlsAreDirty = true;   // signal we need to update tooltips / hovertext
                duckControlsAreDirty = true;
                sendReturnControlsAreDirty = true;
                muteFader.snapToFront();
                soloFader.snapToFront();
                envDuckFader.snapToFront();
                totalSoloCount = 0;
                if (graph) graph->initialize();
                clockSignalFormat = ClockSignalFormat::Default;
            }

            void initialize() override
            {
                MultiTapModule::initialize();
                LoopModule_initialize();
            }

            void updateFlipControls()
            {
                const bool needUpdate = flipControlsAreDirty || (prevFlip != flip);
                if (needUpdate && controlsAreReady)
                {
                    prevFlip = flip;
                    flipControlsAreDirty = false;

                    const char *name = flip ? "Flip" : "Reverse";
                    getInputInfo(controls.revFlipInputId)->name = name;
                    getParamQuantity(controls.revFlipButtonId)->name = name;
                }
            }

            void updateToggleButtonTooltip(int buttonId, const char* offText, const char *onText)
            {
                if (buttonId >= 0 && buttonId < static_cast<int>(params.size()))
                {
                    ParamQuantity* qty = getParamQuantity(buttonId);
                    if (qty)
                        qty->name = (qty->getValue() < 0.5f) ? offText : onText;
                }
            }

            void updateSendReturnControls()
            {
                if (sendReturnControlsAreDirty && controlsAreReady)
                {
                    sendReturnControlsAreDirty = false;

                    updateToggleButtonTooltip(
                        controls.sendReturnButtonId,
                        "Send/return before delay",
                        "Send/return after delay"
                    );
                }
            }

            void updateMuteSoloControls()
            {
                if (controlsAreReady)
                {
                    updateToggleButtonTooltip(controls.muteButtonId, "Mute: OFF", "Mute: ON");
                    updateToggleButtonTooltip(controls.soloButtonId, "Solo: OFF", "Solo: ON");
                }
            }

            void toggleFlip()
            {
                auto action = new BoolToggleAction(flip, flipControlsAreDirty);
                action->redo();
                APP->history->push(action);
            }

            void toggleEnvDuck()
            {
                auto action = new BoolToggleAction(duck, duckControlsAreDirty);
                action->redo();
                APP->history->push(action);
            }

            bool isActivelyClocked() const
            {
                // Actively clocked means both are true:
                // 1. The user enabled clocking on this tape loop.
                // 2. There is a cable connected to the CLOCK input port.
                return (timeKnobInfo.timeMode == TimeMode::ClockSync) && timeKnobInfo.isClockConnected;
            }

            void updateReverseState(int inputId, int buttonParamId, int lightId, float sampleRateHz)
            {
                const bool active = reverseToggleGroup.process();
                if (active)
                    reverseComboSmoother.targetValue = flip ? ReverseFlipMode::Flip : ReverseFlipMode::Reverse;
                else
                    reverseComboSmoother.targetValue = ReverseFlipMode::Forward;
                reverseComboSmoother.process(sampleRateHz);
            }

            json_t* dataToJson() override
            {
                json_t* root = MultiTapModule::dataToJson();
                jsonSetEnum(root, "timeMode", timeKnobInfo.timeMode);
                jsonSetBool(root, "flip", flip);
                jsonSetBool(root, "duck", duck);
                jsonSetBool(root, "polyphonicEnvelopeOutput", polyphonicEnvelopeOutput);
                sendReturnLocationSmoother.jsonSave(root);
                reverseToggleGroup.jsonSave(root);
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                MultiTapModule::dataFromJson(root);
                jsonLoadEnum(root, "timeMode", timeKnobInfo.timeMode);
                jsonLoadBool(root, "flip", flip);
                jsonLoadBool(root, "duck", duck);
                jsonLoadBool(root, "polyphonicEnvelopeOutput", polyphonicEnvelopeOutput);
                sendReturnLocationSmoother.jsonLoad(root);
                reverseToggleGroup.jsonLoad(root);
                updateFlipControls();
            }

            float scaleEnvelope(float env, float sampleRateHz)
            {
                constexpr float limit = 10;      // maximum voltage
                float scale = BicubicLimiter(env, limit);
                envDuckFader.setTarget(duck);
                return envDuckFader.process(sampleRateHz, scale, limit - scale);
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
                            float s = scaleEnvelope(v, sampleRateHz);
                            envOutput.setVoltage(s, c);
                        }
                    }
                    else
                    {
                        float sum = 0;
                        for (int c = 0; c < nc; ++c)
                            sum += audio.sample[c];

                        float v = gain * info[0].env.update(sum, sampleRateHz);
                        float s = scaleEnvelope(v, sampleRateHz);
                        envOutput.setChannels(1);
                        envOutput.setVoltage(s, 0);
                    }
                }
            }

            int updateSolo(Frame& soloAudio, const Frame& rawAudio, int soloButtonParamId, float sampleRateHz)
            {
                soloFader.setTarget(params.at(soloButtonParamId).getValue() > 0.5f);
                float factor = soloFader.process(sampleRateHz, 0, 1);
                if (factor > 0)
                {
                    soloAudio += factor * rawAudio;
                    return 1;
                }
                return 0;
            }

            float updateMuteState(float sampleRateHz, int muteButtonId)
            {
                muteFader.setTarget(params.at(muteButtonId).getValue() > 0.5f);
                return muteFader.process(sampleRateHz, 1, 0);
            }

            bool isAudible() const
            {
                // If there is someone else doing a solo, but not us, they can't hear us!
                if (totalSoloCount>0 && !soloFader.atBack())
                    return false;

                // If we are not muted, we are audible.
                return !muteFader.atBack();
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
                if (sendReturnLocationSmoother.isDelayedActionReady())
                    sendReturnControlsAreDirty = true;

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
                float delayTimeSum = 0;

                const bool isFirstTap = IsEcho(this);
                const bool isSingleTap = isFirstTap && !IsEchoTap(rightExpander.module);

                // In order to have consistent behavior when there is a single tap,
                // parallel mode and serial mode should mean the same thing.
                // It makes the code simpler to pretend like we are in parallel mode in that case.
                const bool parallelMode = isSingleTap || (message.inputRouting == TapInputRouting::Parallel);
                const bool loopback = isFirstTap && backMessage.valid && !parallelMode;
                const bool clocked = isActivelyClocked();

                float feedbackSample = 0;
                int numDeadClocks = 0;
                int numReceivingTriggers = 0;
                for (int c = 0; c < nc; ++c)
                {
                    ChannelInfo& q = info[c];

                    if (controls.clockInputId < 0)
                        vClock = message.clockVoltage.sample[c];
                    else
                        vClock = nextChannelInputVoltage(vClock, controls.clockInputId, c);
                    result.clockVoltage.sample[c] = vClock;

                    // Assume the delay time is in seconds.
                    float delayTime = TwoToPower(controlGroupRawCv(c, cvDelayTime, controls.delayTime, L1, L2));

                    // But it might be a dimensionless clock multiplier instead.
                    if (clocked)
                    {
                        if (clockSignalFormat == ClockSignalFormat::Pulses)
                        {
                            float elapsedSeconds = q.samplesSinceClockTrigger / sampleRateHz;
                            if (q.clockReceiver.updateTrigger(vClock))
                            {
                                q.samplesSinceClockTrigger = 0;
                                if (!q.isReceivingTriggers)
                                    q.isReceivingTriggers = true;
                                else if (elapsedSeconds > TAPELOOP_MAX_DELAY_SECONDS)
                                    q.isReceivingTriggers = false;
                                else
                                    q.clockSyncTime = std::clamp(elapsedSeconds, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
                            }
                            else
                            {
                                ++q.samplesSinceClockTrigger;
                                if (elapsedSeconds > TAPELOOP_MAX_DELAY_SECONDS)
                                    ++numDeadClocks;
                            }

                            if (q.isReceivingTriggers)
                                ++numReceivingTriggers;
                        }
                        else    // assume ClockSignalFormat::Voct
                        {
                            float timeSeconds = TwoToPower(-vClock);    // delay time goes down as RATE increases
                            q.clockSyncTime = std::clamp(timeSeconds, TAPELOOP_MIN_DELAY_SECONDS, TAPELOOP_MAX_DELAY_SECONDS);
                            q.isReceivingTriggers = true;
                            ++numReceivingTriggers;
                        }

                        if (q.clockSyncTime > 0)
                        {
                            // Are musical intervals enabled? If so, snap to closest fraction.
                            if (timeKnobInfo.isMusicalInterval)
                                delayTime = PickClosestFraction(delayTime).value();
                            delayTime *= q.clockSyncTime;
                        }
                    }
                    else
                    {
                        q.samplesSinceClockTrigger = 0;
                        q.isReceivingTriggers = false;
                        q.clockSyncTime = 0;
                    }

                    delayTimeSum += delayTime;
                    q.loop.setDelayTime(delayTime, sampleRateHz);
                    q.loop.setInterpolatorKind(message.interpolatorKind);
                    if (clearSmoother.isDelayedActionReady())
                    {
                        q.loop.clear();
                        if (graph)
                            graph->initialize();
                    }
                    float forward = q.loop.readForward() * clearSmoother.getGain();
                    float reverse = 0;
                    if (reverseComboSmoother.isReverseNeeded())
                        reverse = q.loop.readReverse() * clearSmoother.getGain();
                    q.loop.updateReversePlaybackHead();

                    constexpr float sensitivity = 1.0 / 5.0;  // A 5V change in CV should cause swing of 1 knob unit.
                    float gain = controlGroupRawCv(c, cvGain, controls.gain, 0, 1, sensitivity);
                    reverseComboSmoother.select(
                        forward,
                        reverse,
                        result.envelopeAudio.sample[c],
                        result.chainAudioOutput.sample[c]
                    );

                    if (sendReturnLocationSmoother.currentValue == SendReturnLocation::AfterDelay)
                    {
                        writeSample(srSmooth * result.envelopeAudio.sample[c], sendLeft, sendRight, c, nc, message.polyphonic);
                        result.envelopeAudio.sample[c] = readSample(result.envelopeAudio.sample[c], returnLeft, returnRight, c);
                    }
                    result.envelopeAudio.sample[c] *= srSmooth;

                    result.globalAudioOutput.sample[c] = gain * result.envelopeAudio.sample[c];

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
                        feedbackSample = result.chainAudioOutput.sample[c];
                    }
                    else
                    {
                        // In serial mode, every tap other than the first has NO FEEDBACK;
                        // feedbackSample started at zero and we don't change it.
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

                if (numDeadClocks > 0)
                    timeKnobInfo.deadClockCountdown = static_cast<int>(sampleRateHz / 4);
                else if (timeKnobInfo.deadClockCountdown > 0)
                    --timeKnobInfo.deadClockCountdown;

                timeKnobInfo.isReceivingClockTriggers = (numReceivingTriggers > 0);

                if (graph && nc>0 && delayTimeSum>0)
                {
                    const float meanDelayTime = delayTimeSum / nc;
                    graph->setDelayTime(meanDelayTime);
                    graph->process(inAudio, result.envelopeAudio, sampleRateHz);
                }

                result.globalAudioOutput = panFrame(result.globalAudioOutput);
                recordingLevelOverflow = (unhappyCount > 0);
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
                    constexpr float sensitivity = 1.0 / 5.0;        // 1 knob unit per 5V at 100%
                    float x = getControlValueCustom(controls.pan, -1, +1, sensitivity);
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


        GraphWidget::~GraphWidget()
        {
            if (loopModule)
            {
                // This grapher object is dying, so let the module know it should
                // no longer try to access it. This fixes a crash/hang when deleting
                // EchoTap modules.
                loopModule->graph = nullptr;
            }
        }


        NVGcolor FadeColor(float fade, float intensity, NVGcolor c0, NVGcolor c1)
        {
            NVGcolor cm;
            cm.a = 1;
            cm.r = intensity * ((1-fade)*c0.r + fade*c1.r);
            cm.g = intensity * ((1-fade)*c0.g + fade*c1.g);
            cm.b = intensity * ((1-fade)*c0.b + fade*c1.b);
            return cm;
        }


        void GraphWidget::drawLayer(const DrawArgs& args, int layer)
        {
            const NVGcolor mutedColor = nvgRGB(0x20, 0x40, 0x50);
            const NVGcolor oldInputColor = nvgRGB(0x56, 0xd1, 0x2a);
            const NVGcolor newInputColor = nvgRGB(0xf5, 0xbc, 0x42);

            if (loopModule && layer==1 && currentNumChannels>0 && currentNumChannels<=PORT_MAX_CHANNELS)
            {
                unsigned s = SliceInc(sliceIndex);  // skip currently active slice (not finalized yet)
                for (unsigned k = 0; k < GraphSliceCount; ++k, s = SliceInc(s))
                {
                    NVGcolor inputColor = mutedColor;
                    NVGcolor outputColor = mutedColor;
                    if (loopModule->isAudible())
                    {
                        const float fade = static_cast<float>(k) / static_cast<float>(GraphSliceCount+1);
                        inputColor  = FadeColor(fade, 1.0, oldInputColor, newInputColor);
                        outputColor = FadeColor(fade, 1.0, SCHEME_PURPLE, SCHEME_CYAN);
                    }
                    drawPowerFrame(args.vg, sliceArray.at(s).sumInput,  k, inputColor,  -1,  0);
                    drawPowerFrame(args.vg, sliceArray.at(s).sumOutput, k, outputColor,  0, +1);
                }
            }
            OpaqueWidget::drawLayer(args, layer);
        }


        void GraphWidget::drawPowerFrame(NVGcontext* vg, const Frame& power, int sliceIndex, NVGcolor color, float leftLimit, float rightLimit)
        {
            // Draw a horizontal line segment for each channel at the corresponding y-coordinate.
            // Use a bicubic limiter to keep the numbers inside a desired range.

            constexpr float strokeWidthPx = 0.5;
            for (int c = 0; c < currentNumChannels; ++c)
            {
                float zs = power.sample[c] * zoom;
                float p = BicubicLimiter<float>(zs, GraphVoltageLimit) / GraphVoltageLimit;
                p = std::max(ThinLineFraction, p);
                Vec left   = position(sliceIndex, c, p*leftLimit);
                Vec right  = position(sliceIndex, c, p*rightLimit);
                nvgBeginPath(vg);
                nvgLineCap(vg, NVG_BUTT);
                nvgStrokeWidth(vg, strokeWidthPx);
                nvgStrokeColor(vg, color);
                nvgMoveTo(vg, left.x, left.y);
                nvgLineTo(vg, right.x, right.y);
                nvgStroke(vg);
            }
        }


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
            LoopModule* loopModule{};
            const std::string chainFontPath = asset::system("res/fonts/DejaVuSans.ttf");
            const float mmShiftFirstTap = (PanelWidth("echo") - PanelWidth("echotap")) / 2;
            const float mmModeButtonRadius = 3.5;
            const float mmChainIndexCenterY = 4.5;
            SvgOverlay* revLabel = nullptr;
            SvgOverlay* revSelLabel = nullptr;
            SvgOverlay* flpLabel = nullptr;
            SvgOverlay* flpSelLabel = nullptr;
            SvgOverlay* envLabel = nullptr;
            SvgOverlay* envSelLabel = nullptr;
            SvgOverlay* invLabel = nullptr;
            SvgOverlay* invSelLabel = nullptr;
            Vec flpRevLabelPos;
            Vec envDuckLabelPos;
            float dxFlipRev{};
            float dyFlipRev{};
            bool hilightInputRoutingButton = false;
            bool hilightRevFlipButton = false;
            bool hilightEnvDuckButton = false;
            SapphireTooltip* routingTooltip = nullptr;
            SapphireTooltip* revFlipTooltip = nullptr;
            SapphireTooltip* envDuckTooltip = nullptr;

            explicit LoopWidget(
                const std::string& moduleCode,
                LoopModule* lmod,
                const std::string& panelSvgFileName,
                const std::string& revSvgFileName,
                const std::string& revSelSvgFileName,
                const std::string& flpSvgFileName,
                const std::string& flpSelSvgFileName,
                const std::string& envSvgFileName,
                const std::string& envSelSvgFileName,
                const std::string& invSvgFileName,
                const std::string& invSelSvgFileName
            )
                : MultiTapWidget(moduleCode, panelSvgFileName)
                , loopModule(lmod)
            {
                revLabel    = addLabelOverlay(revSvgFileName);
                revSelLabel = addLabelOverlay(revSelSvgFileName);
                flpLabel    = addLabelOverlay(flpSvgFileName);
                flpSelLabel = addLabelOverlay(flpSelSvgFileName);
                envLabel    = addLabelOverlay(envSvgFileName);
                envSelLabel = addLabelOverlay(envSelSvgFileName);
                invLabel    = addLabelOverlay(invSvgFileName);
                invSelLabel = addLabelOverlay(invSelSvgFileName);

                ComponentLocation centerLoc = FindComponent(modcode, "label_flp_rev");
                flpRevLabelPos = Vec(mm2px(centerLoc.cx), mm2px(centerLoc.cy));

                centerLoc = FindComponent(modcode, "label_env_duck");
                envDuckLabelPos = Vec(mm2px(centerLoc.cx), mm2px(centerLoc.cy));

                ComponentLocation inputLoc  = FindComponent(modcode, "reverse_input");
                ComponentLocation buttonLoc = FindComponent(modcode, "reverse_button");
                const float dxCushion = 8.0;
                dxFlipRev = mm2px(buttonLoc.cx - inputLoc.cx - dxCushion) / 2;
                dyFlipRev = mm2px(2.5);

                addGraphWidget(lmod);
            }

            virtual ~LoopWidget()
            {
                destroyTooltip(routingTooltip);
                destroyTooltip(revFlipTooltip);
                destroyTooltip(envDuckTooltip);
            }

            SvgOverlay* addLabelOverlay(const std::string& svgFileName)
            {
                SvgOverlay* overlay = SvgOverlay::Load(svgFileName);
                addChild(overlay);
                overlay->hide();
                return overlay;
            }

            virtual void resetTapAction() = 0;

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
                ToggleGroup* group = loopModule ? &(loopModule->reverseToggleGroup) : nullptr;

                addToggleGroup(
                    group,
                    "reverse",
                    inputId,
                    buttonParamId,
                    buttonLightId,
                    '\0',
                    7.0,
                    SCHEME_PURPLE
                );
            }

            void addGraphWidget(LoopModule* lmod)
            {
                ComponentLocation upperLeft  = FindComponent(modcode, "graph_upper_left");
                ComponentLocation lowerRight = FindComponent(modcode, "graph_lower_right");
                auto graph = new GraphWidget(lmod, upperLeft.cx, upperLeft.cy, lowerRight.cx, lowerRight.cy);
                if (lmod)
                    lmod->graph = graph;
                addChild(graph);
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

            void updateFlipReverse(const LoopModule* lmod)
            {
                flpLabel->setVisible(lmod->flip && !hilightRevFlipButton);
                flpSelLabel->setVisible(lmod->flip && hilightRevFlipButton);
                revLabel->setVisible(!lmod->flip && !hilightRevFlipButton);
                revSelLabel->setVisible(!lmod->flip && hilightRevFlipButton);
            }

            void updateEnvDuck(const LoopModule* lmod)
            {
                envLabel->setVisible(!lmod->duck && !hilightEnvDuckButton);
                envSelLabel->setVisible(!lmod->duck && hilightEnvDuckButton);
                invLabel->setVisible(lmod->duck && !hilightEnvDuckButton);
                invSelLabel->setVisible(lmod->duck && hilightEnvDuckButton);
            }

            void step() override
            {
                MultiTapWidget::step();
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    lmod->hideLeftBorder  = isConnectedOnLeft();
                    lmod->hideRightBorder = isConnectedOnRight();
                    lmod->updateFlipControls();
                    lmod->updateSendReturnControls();
                    lmod->updateMuteSoloControls();
                    updateFlipReverse(lmod);
                    updateEnvDuck(lmod);
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

            bool isInsideEnvDuckButton(Vec pos) const
            {
                const float dx = pos.x - envDuckLabelPos.x;
                const float dy = pos.y - envDuckLabelPos.y;
                const float rectWidth = mm2px(8.0);
                const float rectHeight = mm2px(4.5);
                return (std::abs(dx) <= rectWidth/2) && (std::abs(dy) <= rectHeight/2);
            }

            virtual void onMousePress(const ButtonEvent& e)
            {
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    if (offerRoutingModeChange() && isInsideInputRoutingButton(e.pos))
                        lmod->bumpTapInputRouting();

                    if (isInsideFlipRevButton(e.pos))
                        lmod->toggleFlip();

                    if (isInsideEnvDuckButton(e.pos))
                        lmod->toggleEnvDuck();
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

            void updateEnvDuckButton(bool state)
            {
                updateTooltip(hilightEnvDuckButton, state, envDuckTooltip, "Toggle envelope follow/duck");
            }

            void onHover(const HoverEvent& e) override
            {
                updateRoutingButton(isInsideInputRoutingButton(e.pos));
                updateFlipRevButton(isInsideFlipRevButton(e.pos));
                updateEnvDuckButton(isInsideEnvDuckButton(e.pos));
                MultiTapWidget::onHover(e);
            }

            void onLeave(const LeaveEvent& e) override
            {
                updateRoutingButton(false);
                updateFlipRevButton(false);
                updateEnvDuckButton(false);
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

                        if (lmod->recordingLevelOverflow)
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
                    lmod->reverseToggleGroup.addMenuItems(menu);
                }
            }

            void addTimeControlGroup(int paramId, int attenId, int cvInputId)
            {
                TimeKnob* timeKnob = addSapphireFlatControlGroup<TimeKnob>("time", paramId, attenId, cvInputId);
                auto lmod = dynamic_cast<LoopModule*>(module);
                if (lmod)
                {
                    lmod->timeKnob = timeKnob;
                    timeKnob->info = &(lmod->timeKnobInfo);
                }
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

            void addMuteSoloButtons(int muteButtonId, int soloButtonId)
            {
                auto muteButton = createParamCentered<MuteButton>(Vec{}, module, muteButtonId);
                muteButton->loopWidget = this;
                addSapphireParam(muteButton, "mute_button");

                auto soloButton = createParamCentered<SoloButton>(Vec{}, module, soloButtonId);
                soloButton->loopWidget = this;
                addSapphireParam(soloButton, "solo_button");
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
            { 1,  8, "1/32 note"   },   //  0
            { 1,  6, "1/16 trip"   },   //  1
            { 3, 16, "1/32 dot"    },   //  2
            { 1,  4, "1/16 note"   },   //  3
            { 1,  3, "1/8 trip"    },   //  4
            { 3,  8, "1/16 dot"    },   //  5
            { 1,  2, "1/8 note"    },   //  6
            { 2,  3, "1/4 trip"    },   //  7
            { 3,  4, "1/8 dot"     },   //  8
            { 1,  1, "1/4 note"    },   //  9
            { 4,  3, "1/2 trip"    },   // 10
            { 3,  2, "1/4 dot"     },   // 11
            { 2,  1, "1/2 note"    },   // 12
            { 3,  1, "1/2 dot"     },   // 13
            { 4,  1, "whole note"  },   // 14
            { 6,  1, "whole dot"   },   // 15
            { 8,  1, "double note" },   // 16
        };


        const Fraction& PickClosestFraction(float ratio)
        {
            // Hand-rolled decision tree.

            if (!std::isfinite(ratio) || ratio > 8.0)
                return MusicalFractions.back();

            if (ratio <= 0.0)
                return MusicalFractions.front();

            int index = 0;
            const float x = ratio * 48;     // least common multiple for our musical denominators
            if (x < 42)
            {
                if (x < 17)
                {
                    if (x < 10.5)
                    {
                        if (x < 8.5)
                            index = (x < 7) ? 0 : 1;
                        else
                            index = 2;
                    }
                    else
                    {
                        index = (x < 14) ? 3 : 4;
                    }
                }
                else
                {
                    if (x < 28)
                        index = (x < 21) ? 5 : 6;
                    else
                        index = (x < 34) ? 7 : 8;
                }
            }
            else
            {
                if (x < 120)
                {
                    if (x < 68)
                        index = (x < 56) ? 9 : 10;
                    else
                        index = (x < 84) ? 11 : 12;
                }
                else
                {
                    if (x < 240)
                        index = (x < 168) ? 13 : 14;
                    else
                        index = (x < 336) ? 15 : 16;
                }
            }
            return MusicalFractions.at(index);
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
                    if (lmod->timeKnobInfo.isMusicalInterval)
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
                INPUT_MODE_BUTTON_PARAM,
                MUTE_BUTTON_PARAM,
                SOLO_BUTTON_PARAM,
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
                ToggleGroup freezeToggleGroup;
                AnimatedTriggerReceiver clearReceiver;
                RoutingSmoother routingSmoother;
                InterpolatorKind interpolatorKind{};
                Crossfader freezeFader;
                PortLabelMode inputLabels{};
                bool autoCreateOutputModule = true;

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
                    reverseToggleGroup.config(this, "Reverse/flip", "reverseToggleGroup", REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, "Reverse", "Reverse gate");
                    freezeToggleGroup.config(this, "Freeze", "freezeToggleGroup", FREEZE_INPUT, FREEZE_BUTTON_PARAM, FREEZE_BUTTON_LIGHT, "Freeze", "Freeze gate");
                    configToggleGroup(CLEAR_INPUT, CLEAR_BUTTON_PARAM, "Clear", "Clear trigger");
                    configInput(CLOCK_INPUT, "Clock");
                    configButton(CLOCK_BUTTON_PARAM, "Toggle all clock sync");
                    configButton(INTERVAL_BUTTON_PARAM, "Snap to musical intervals");
                    configButton(SEND_RETURN_BUTTON_PARAM);     // tooltip changed dynamically
                    configButton(INIT_CHAIN_BUTTON_PARAM, "Initialize entire chain");
                    configButton(INIT_TAP_BUTTON_PARAM, "Initialize this tap only");
                    configButton(INPUT_MODE_BUTTON_PARAM);      // tooltip changed dynamically
                    configButton(MUTE_BUTTON_PARAM);            // tooltip changed dynamically
                    configButton(SOLO_BUTTON_PARAM);            // tooltip changed dynamically
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    addDcRejectQuantity(DC_REJECT_PARAM, 20);
                    EchoModule_initialize();
                    controlsAreReady = true;
                }

                void EchoModule_initialize()
                {
                    routingSmoother.initialize();
                    interpolatorKind = InterpolatorKind::Linear;
                    freezeToggleGroup.initialize();
                    clearReceiver.initialize();
                    dcRejectQuantity->initialize();
                    freezeFader.snapToFront();      // front=false=0, back=true=1
                }

                void resetTap()
                {
                    LoopModule_initialize();
                    params.at(TIME_PARAM).setValue(0);
                    params.at(TIME_ATTEN).setValue(0);
                    params.at(PAN_PARAM).setValue(0);
                    params.at(PAN_ATTEN).setValue(0);
                    params.at(GAIN_PARAM).setValue(1);
                    params.at(GAIN_ATTEN).setValue(0);
                    params.at(REVERSE_BUTTON_PARAM).setValue(0);
                    params.at(ENV_GAIN_PARAM).setValue(1);
                    params.at(SEND_RETURN_BUTTON_PARAM).setValue(0);
                    params.at(MUTE_BUTTON_PARAM).setValue(0);
                    params.at(SOLO_BUTTON_PARAM).setValue(0);
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
                    controls.muteButtonId = MUTE_BUTTON_PARAM;
                    controls.soloButtonId = SOLO_BUTTON_PARAM;
                }

                bool polyphonicMode()
                {
                    return getParamQuantity(INPUT_MODE_BUTTON_PARAM)->getValue() > 0.5f;
                }

                void process(const ProcessArgs& args) override
                {
                    Message outMessage;
                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();
                    totalSoloCount = inBackMessage.soloCount;
                    timeKnobInfo.isMusicalInterval = outMessage.musicalInterval = (params.at(INTERVAL_BUTTON_PARAM).getValue() > 0.5);
                    outMessage.routingSmooth = routingSmoother.process(args.sampleRate);
                    outMessage.inputRouting = routingSmoother.currentValue;
                    outMessage.polyphonic = polyphonicMode();
                    outMessage.freezeMix = updateFreezeState(args.sampleRate);
                    updateReverseState(REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, args.sampleRate);
                    outMessage.clear = updateClearState(args.sampleRate);
                    outMessage.chainIndex = 2;
                    outMessage.originalAudio = readOriginalAudio(args.sampleRate, outMessage.polyphonic, inputLabels);
                    outMessage.feedback = getFeedbackPoly();
                    timeKnobInfo.isClockConnected = outMessage.isClockConnected = inputs.at(CLOCK_INPUT).isConnected();
                    outMessage.interpolatorKind = interpolatorKind;
                    TapeLoopResult result = updateTapeLoops(outMessage.originalAudio, args.sampleRate, outMessage, inBackMessage);
                    result.globalAudioOutput *= updateMuteState(args.sampleRate, MUTE_BUTTON_PARAM);
                    outMessage.chainAudio = result.chainAudioOutput;
                    outMessage.summedAudio = result.globalAudioOutput;
                    outMessage.soloCount = updateSolo(outMessage.soloAudio, result.globalAudioOutput, SOLO_BUTTON_PARAM, args.sampleRate);
                    outMessage.clockVoltage = result.clockVoltage;
                    outMessage.neonMode = neonMode;
                    outMessage.clockSignalFormat = clockSignalFormat;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, result.envelopeAudio);
                    sendMessage(outMessage);
                }

                Frame readOriginalAudio(float sampleRateHz, bool polyphonic, PortLabelMode& mode)
                {
                    Frame audio = readFrame(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, polyphonic, mode);
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
                    freezeToggleGroup.jsonSave(root);
                    jsonSetEnum(root, "interpolatorKind", interpolatorKind);
                    jsonSetEnum(root, "clockSignalFormat", clockSignalFormat);
                    jsonSetBool(root, "autoCreateOutputModule", autoCreateOutputModule);
                    return root;
                }

                void dataFromJson(json_t* root) override
                {
                    LoopModule::dataFromJson(root);
                    routingSmoother.jsonLoad(root);
                    freezeToggleGroup.jsonLoad(root);
                    jsonLoadEnum(root, "interpolatorKind", interpolatorKind);
                    jsonLoadEnum(root, "clockSignalFormat", clockSignalFormat);
                    jsonLoadBool(root, "autoCreateOutputModule", autoCreateOutputModule);
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
                    constexpr float sensitivity = 0.5 / 5.0;    // half of knob range per 5V change in CV
                    for (int c = 0; c < feedback.nchannels; ++c)
                    {
                        nextChannelInputVoltage(cv, FEEDBACK_CV_INPUT, c);
                        feedback.sample[c] = maxFeedbackRatio * cvGetVoltPerOctave(FEEDBACK_PARAM, FEEDBACK_ATTEN, cv * sensitivity, 0, 1);
                    }
                    return feedback;
                }

                float updateFreezeState(float sampleRateHz)
                {
                    const bool active = freezeToggleGroup.process();
                    freezeFader.setTarget(active);
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
                SvgOverlay* clockLabel = nullptr;
                SvgOverlay* clockSelLabel = nullptr;
                SvgOverlay* rateLabel = nullptr;
                SvgOverlay* rateSelLabel = nullptr;
                Vec clockLabelPos;
                bool isMouseInsideClockLabel = false;
                bool hilightClockRateButton = false;
                SapphireTooltip* clockRateTooltip = nullptr;

                explicit EchoWidget(EchoModule* module)
                    : LoopWidget(
                        "echo",
                        module,
                        asset::plugin(pluginInstance, "res/echo.svg"),
                        asset::plugin(pluginInstance, "res/echo_rev.svg"),
                        asset::plugin(pluginInstance, "res/echo_rev_sel.svg"),
                        asset::plugin(pluginInstance, "res/echo_flp.svg"),
                        asset::plugin(pluginInstance, "res/echo_flp_sel.svg"),
                        asset::plugin(pluginInstance, "res/echo_env.svg"),
                        asset::plugin(pluginInstance, "res/echo_env_sel.svg"),
                        asset::plugin(pluginInstance, "res/echo_dck.svg"),
                        asset::plugin(pluginInstance, "res/echo_dck_sel.svg")
                    )
                    , echoModule(module)
                {
                    splash.x1 = 6 * HP_MM;
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addLabelOverlays();

                    // Global controls/ports
                    addStereoInputPorts(AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT, "audio");
                    addSapphireControlGroup("feedback", FEEDBACK_PARAM, FEEDBACK_ATTEN, FEEDBACK_CV_INPUT);
                    addFreezeToggleGroup();
                    addClearTriggerGroup();
                    addSapphireInput(CLOCK_INPUT, "clock_input");
                    addClockButton();
                    addIntervalButton();
                    addInitChainButton();
                    addInputModeButton();

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
                    addMuteSoloButtons(MUTE_BUTTON_PARAM, SOLO_BUTTON_PARAM);

                    ComponentLocation labelLoc = FindComponent(modcode, "clock_label");
                    clockLabelPos = Vec(mm2px(labelLoc.cx), mm2px(labelLoc.cy));
                }

                virtual ~EchoWidget()
                {
                    destroyTooltip(clockRateTooltip);
                }

                bool isInsideClockLabel(Vec pos) const
                {
                    const float dx = pos.x - clockLabelPos.x;
                    const float dy = pos.y - clockLabelPos.y;
                    const float width  = mm2px(12.0);
                    const float height = mm2px(4.0);
                    return (std::abs(dx) < width/2) && (std::abs(dy) < height/2);
                }

                void addLabelOverlays()
                {
                    clockLabel    = addLabelOverlay(asset::plugin(pluginInstance, "res/echo_clock.svg"));
                    clockSelLabel = addLabelOverlay(asset::plugin(pluginInstance, "res/echo_clock_sel.svg"));
                    rateLabel     = addLabelOverlay(asset::plugin(pluginInstance, "res/echo_voct.svg"));
                    rateSelLabel  = addLabelOverlay(asset::plugin(pluginInstance, "res/echo_voct_sel.svg"));
                }

                void updateClockRateButton(bool state)
                {
                    updateTooltip(hilightClockRateButton, state, clockRateTooltip, "Toggle input format: CLOCK/RATE");
                }

                void onHover(const HoverEvent& e) override
                {
                    LoopWidget::onHover(e);
                    isMouseInsideClockLabel = isInsideClockLabel(e.pos);
                }

                void onLeave(const LeaveEvent& e) override
                {
                    LoopWidget::onLeave(e);
                    isMouseInsideClockLabel = false;
                }

                void resetTapAction() override
                {
                    if (!echoModule)
                        return;

                    // Preserve state before reset.
                    auto h = new history::ModuleChange;
                    h->name = "Initialize Echo tap";
                    h->moduleId = echoModule->id;
                    h->oldModuleJ = echoModule->toJson();

                    // Reset tap controls only.
                    echoModule->resetTap();

                    h->newModuleJ = echoModule->toJson();
                    APP->history->push(h);
                }

                void addInputModeButton()
                {
                    auto button = createParamCentered<InputModeButton>(Vec{}, echoModule, INPUT_MODE_BUTTON_PARAM);
                    button->echoWidget = this;
                    addSapphireParam(button, "input_mode_button");
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
                    ToggleGroup* group = echoModule ? &(echoModule->freezeToggleGroup) : nullptr;

                    addToggleGroup(
                        group,
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
                        nullptr,
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

                    if (layer==1 && isClockPortConnected())
                    {
                        NVGcolor color = echoModule->timeKnobInfo.color();
                        drawClockSyncSymbol(args.vg, color, 1.25);
                    }
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

                void onMousePress(const ButtonEvent& e) override
                {
                    LoopWidget::onMousePress(e);
                    if (echoModule)
                    {
                        if (isInsideClockLabel(e.pos))
                            echoModule->clockSignalFormat = NextEnumValue(echoModule->clockSignalFormat);
                    }
                }

                void step() override
                {
                    LoopWidget::step();

                    if (echoModule)
                    {
                        echoModule->updateToggleButtonTooltip(INPUT_MODE_BUTTON_PARAM, "Stereo mode", "Polyphonic mode");

                        clockLabel   ->setVisible(!isMouseInsideClockLabel && echoModule->clockSignalFormat == ClockSignalFormat::Pulses);
                        clockSelLabel->setVisible( isMouseInsideClockLabel && echoModule->clockSignalFormat == ClockSignalFormat::Pulses);
                        rateLabel    ->setVisible(!isMouseInsideClockLabel && echoModule->clockSignalFormat == ClockSignalFormat::Voct);
                        rateSelLabel ->setVisible( isMouseInsideClockLabel && echoModule->clockSignalFormat == ClockSignalFormat::Voct);

                        updateClockRateButton(isMouseInsideClockLabel);

                        // Automatically add an EchoOut expander when we first insert Echo.
                        // But we have to wait more than one step call, because otherwise
                        // it screws up the undo/redo history stack.

                        if (echoModule->autoCreateOutputModule && (creationCountdown > 0))
                        {
                            --creationCountdown;
                            if (creationCountdown == 0)
                            {
                                echoModule->autoCreateOutputModule = false;     // prevent creating another EchoOut when patch is loaded again
                                if (!IsEchoReceiver(module->rightExpander.module))
                                    AddExpander(modelSapphireEchoOut, this, ExpanderDirection::Right);
                            }
                        }
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

                        echoModule->freezeToggleGroup.addMenuItems(menu);
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
                            return lmod->timeKnobInfo.timeMode == TimeMode::ClockSync;
                        }
                    );

                    const TimeMode timeMode = (
                        (2*clockCount > totalCount) ?
                        TimeMode::Seconds :
                        TimeMode::ClockSync
                    );

                    visitTaps([=](LoopModule *lmod)
                    {
                        lmod->timeKnobInfo.timeMode = timeMode;
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
            if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS)
            {
                if (loopWidget)
                    loopWidget->resetTapAction();
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
                MUTE_BUTTON_PARAM,
                SOLO_BUTTON_PARAM,
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
                    configButton(SEND_RETURN_BUTTON_PARAM);     // tooltip changed dynamically
                    configTimeControls(TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
                    configPanControls(PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                    configGainControls(GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
                    reverseToggleGroup.config(this, "Reverse/flip", "reverseToggleGroup", REVERSE_INPUT, REVERSE_BUTTON_PARAM, REVERSE_BUTTON_LIGHT, "Reverse", "Reverse gate");
                    configParam(ENV_GAIN_PARAM, 0, 2, 1, "Envelope follower gain", " dB", -10, 20*4);
                    configButton(INIT_CHAIN_BUTTON_PARAM, "Initialize entire chain");
                    configButton(INIT_TAP_BUTTON_PARAM, "Initialize this tap only");
                    configButton(MUTE_BUTTON_PARAM);    // tooltip changed dynamically
                    configButton(SOLO_BUTTON_PARAM);    // tooltip changed dynamically
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
                    controls.muteButtonId = MUTE_BUTTON_PARAM;
                    controls.soloButtonId = SOLO_BUTTON_PARAM;
                }

                void copyParamFrom(Echo::EchoModule* echoModule, EchoTap::ParamId targetId, Echo::ParamId sourceId)
                {
                    float x = echoModule->params.at(sourceId).getValue();
                    params.at(targetId).setValue(x);
                }

                void tryCopySettingsFrom(SapphireModule* other) override
                {
                    auto emod = dynamic_cast<Echo::EchoModule*>(other);
                    if (emod)
                    {
                        timeKnobInfo = emod->timeKnobInfo;
                        polyphonicEnvelopeOutput = emod->polyphonicEnvelopeOutput;
                        flip = emod->flip;
                        duck = emod->duck;
                        clockSignalFormat = emod->clockSignalFormat;
                        flipControlsAreDirty = true;
                        sendReturnControlsAreDirty = true;
                        copyParamFrom(emod, TIME_PARAM, Echo::TIME_PARAM);
                        copyParamFrom(emod, TIME_ATTEN, Echo::TIME_ATTEN);
                        copyParamFrom(emod, PAN_PARAM, Echo::PAN_PARAM);
                        copyParamFrom(emod, PAN_ATTEN, Echo::PAN_ATTEN);
                        copyParamFrom(emod, SEND_RETURN_BUTTON_PARAM, Echo::SEND_RETURN_BUTTON_PARAM);
                        copyParamFrom(emod, GAIN_PARAM, Echo::GAIN_PARAM);
                        copyParamFrom(emod, GAIN_ATTEN, Echo::GAIN_ATTEN);
                        copyParamFrom(emod, REVERSE_BUTTON_PARAM, Echo::REVERSE_BUTTON_PARAM);
                        copyParamFrom(emod, ENV_GAIN_PARAM, Echo::ENV_GAIN_PARAM);
                        copyParamFrom(emod, MUTE_BUTTON_PARAM, Echo::MUTE_BUTTON_PARAM);
                        copyParamFrom(emod, SOLO_BUTTON_PARAM, Echo::SOLO_BUTTON_PARAM);
                    }
                }

                void process(const ProcessArgs& args) override
                {
                    const Message inMessage = receiveMessageOrDefault();
                    const BackwardMessage inBackMessage = receiveBackwardMessageOrDefault();

                    // Copy input to output by default, then patch whatever is different.
                    Message outMessage = inMessage;
                    timeKnobInfo.isMusicalInterval = inMessage.musicalInterval;
                    chainIndex = inMessage.chainIndex;
                    timeKnobInfo.isClockConnected = inMessage.isClockConnected;
                    includeNeonModeMenuItem = !inMessage.valid;
                    clockSignalFormat = inMessage.clockSignalFormat;

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
                    result.globalAudioOutput *= updateMuteState(args.sampleRate, MUTE_BUTTON_PARAM);
                    outMessage.chainAudio = result.chainAudioOutput;
                    outMessage.summedAudio += result.globalAudioOutput;
                    outMessage.soloCount += updateSolo(outMessage.soloAudio, result.globalAudioOutput, SOLO_BUTTON_PARAM, args.sampleRate);
                    outMessage.clockVoltage = result.clockVoltage;
                    updateEnvelope(ENV_OUTPUT, ENV_GAIN_PARAM, args.sampleRate, result.envelopeAudio);
                    sendMessage(outMessage);

                    BackwardMessage outBackMessage;
                    if (inBackMessage.valid)
                    {
                        // We received a backward message from the right, so just copy it.
                        outBackMessage = inBackMessage;
                    }
                    else
                    {
                        // I am the final EchoTap module, the ultimate source of truth!
                        outBackMessage.loopAudio = result.chainAudioOutput;
                        outBackMessage.soloCount = outMessage.soloCount;
                    }
                    totalSoloCount = outBackMessage.soloCount;
                    sendBackwardMessage(outBackMessage);
                }
            };


            struct EchoTapWidget : LoopWidget
            {
                EchoTapModule* echoTapModule{};

                explicit EchoTapWidget(EchoTapModule* module)
                    : LoopWidget(
                        "echotap",
                        module,
                        asset::plugin(pluginInstance, "res/echotap.svg"),
                        asset::plugin(pluginInstance, "res/echotap_rev.svg"),
                        asset::plugin(pluginInstance, "res/echotap_rev_sel.svg"),
                        asset::plugin(pluginInstance, "res/echotap_flp.svg"),
                        asset::plugin(pluginInstance, "res/echotap_flp_sel.svg"),
                        asset::plugin(pluginInstance, "res/echotap_env.svg"),
                        asset::plugin(pluginInstance, "res/echotap_env_sel.svg"),
                        asset::plugin(pluginInstance, "res/echotap_dck.svg"),
                        asset::plugin(pluginInstance, "res/echotap_dck_sel.svg")
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
                    addMuteSoloButtons(MUTE_BUTTON_PARAM, SOLO_BUTTON_PARAM);
                }

                bool isConnectedOnLeft() const override
                {
                    return module && IsEchoSender(module->leftExpander.module);
                }

                void resetTapAction() override
                {
                    resetAction();
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
                Crossfader firstSoloFader;      // crossfades the transition between muting everyone else or not

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
                    firstSoloFader.snapToFront();
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

                    firstSoloFader.setTarget(message.soloCount > 0);
                    float solo = firstSoloFader.process(args.sampleRate, 0, 1);

                    constexpr float gainSensitivity = 1.0 / 5.0;    // one knob unit per 5V change in CV
                    constexpr float mixSensitivity  = 0.5 / 5.0;    // half a knob unit per 5V change in CV
                    float cvLevel = 0;
                    float cvMix = 0;
                    for (int c = 0; c < audio.nchannels; ++c)
                    {
                        nextChannelInputVoltage(cvLevel, GLOBAL_LEVEL_CV_INPUT, c);
                        float gain = Cube(cvGetVoltPerOctave(GLOBAL_LEVEL_PARAM, GLOBAL_LEVEL_ATTEN, cvLevel * gainSensitivity, 0, 2));

                        nextChannelInputVoltage(cvMix, GLOBAL_MIX_CV_INPUT, c);
                        float mix = cvGetVoltPerOctave(GLOBAL_MIX_PARAM, GLOBAL_MIX_ATTEN, cvMix * mixSensitivity, 0, 1);

                        float wetSample = LinearMix(
                            solo,
                            message.summedAudio.sample[c],
                            message.soloAudio.sample[c]
                        );

                        audio.sample[c] = gain * LinearMix(
                            mix,
                            message.originalAudio.sample[c],
                            wetSample
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
