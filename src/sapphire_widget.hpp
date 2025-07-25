/*
    sapphire_widget.hpp  -  Don Cross <cosinekitty@gmail.com>
    https://github.com/cosinekitty/sapphire
*/

#pragma once
#include "plugin.hpp"
#include "sapphire_panel.hpp"

namespace Sapphire
{
    constexpr float HP_MM = 5.08;
    constexpr float PANEL_HEIGHT_MM = 128.5;

    inline int hpDistance(float mm)
    {
        return static_cast<int>(std::round(mm / HP_MM));
    }

    inline int railDistance(float mm)
    {
        return static_cast<int>(std::round(mm / PANEL_HEIGHT_MM));
    }

    inline float px2mm(float px)
    {
        constexpr float factor = 25.4 / 75;
        return px * factor;
    }

    inline Vec mm_to_px(const ComponentLocation& loc)
    {
        return Vec(mm2px(loc.cx), mm2px(loc.cy));
    }

    inline bool OneShotCountdown(int& counter)
    {
        return (counter > 0) && (--counter == 0);
    }


    struct BoolToggleAction : history::Action
    {
        bool& flag;

        explicit BoolToggleAction(bool& _flag, const std::string& toggledThing)
            : flag(_flag)
        {
            name = "toggle " + toggledThing;
        }

        void toggle()
        {
            flag = !flag;
        }

        void undo() override
        {
            toggle();
        }

        void redo() override
        {
            toggle();
        }

        static MenuItem* CreateMenuItem(
            bool& flag,
            const std::string& menuItemText,
            const std::string& toggledThing
        );

        static void AddMenuItem(
            Menu* menu,
            bool& flag,
            const std::string& menuItemText,
            const std::string& toggledThing)
        {
            if (menu)
                menu->addChild(CreateMenuItem(flag, menuItemText, toggledThing));
        }
    };


    template <typename base_knob_t>
    struct OutputLimiterKnob : base_knob_t, RemovalSubscriber
    {
        SapphireModule* sapphireModule{};
        WarningLightWidget* warningLight{};

        void appendContextMenu(Menu* menu) override
        {
            base_knob_t::appendContextMenu(menu);
            if (sapphireModule)
            {
                menu->addChild(new MenuSeparator);
                menu->addChild(sapphireModule->createLimiterWarningLightMenuItem());
            }
        }

        void disconnect() override
        {
            sapphireModule = nullptr;
            warningLight = nullptr;
        }

        void onRemove(const Widget::RemoveEvent& e) override
        {
            if (sapphireModule)
                sapphireModule->unsubscribe(this);

            base_knob_t::onRemove(e);
        }
    };


    using OutputLimiterLargeKnob = OutputLimiterKnob<RoundLargeBlackKnob>;
    using OutputLimiterSmallKnob = OutputLimiterKnob<RoundSmallBlackKnob>;


    struct SapphireAttenuverterKnob : Trimpot
    {
        bool* lowSensitivityMode = nullptr;

        void appendContextMenu(Menu* menu) override
        {
            Trimpot::appendContextMenu(menu);
            if (lowSensitivityMode)
            {
                menu->addChild(createBoolMenuItem(
                    "Low sensitivity",
                    "",
                    [=]() -> bool       // getter
                    {
                        return *lowSensitivityMode;
                    },
                    [=](bool state)     // setter
                    {
                        if (state != *lowSensitivityMode)
                            InvokeAction(new BoolToggleAction(*lowSensitivityMode, "attenuverter sensitivity"));
                    }
                ));
            }
        }

        bool isLowSensitive() const
        {
            return lowSensitivityMode && *lowSensitivityMode;
        }

        void setLowSensitive(bool s)
        {
            if (lowSensitivityMode)
                *lowSensitivityMode = s;
        }

        void drawLayer(const DrawArgs& args, int layer) override
        {
            Trimpot::drawLayer(args, layer);

            if (layer == 1)
            {
                if (isLowSensitive())
                {
                    // Draw a small dot on top of the knob to indicate that it is in low-sensitivity mode.
                    nvgBeginPath(args.vg);
                    nvgStrokeColor(args.vg, SCHEME_RED);
                    nvgFillColor(args.vg, SCHEME_ORANGE);
                    const float dotRadius = 3.5f;
                    nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, dotRadius);
                    nvgStroke(args.vg);
                    nvgFill(args.vg);
                }
            }
        }
    };


    struct SapphireChannelDisplay : Widget
    {
        SapphireModule* module = nullptr;
        std::string fontPath = asset::system("res/fonts/DSEG7ClassicMini-BoldItalic.ttf");
        std::string text;
        float fontSize = 16;
        NVGcolor fgColor = SCHEME_ORANGE;
        Vec textPos{22, 20};

        void prepareFont(const DrawArgs& args)
        {
            std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
            if (!font)
                return;
            nvgFontFaceId(args.vg, font->handle);
            nvgFontSize(args.vg, fontSize);
            nvgTextLetterSpacing(args.vg, 0.0);
            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);
        }

        void draw(const DrawArgs& args) override
        {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2);
            nvgFillColor(args.vg, nvgRGB(0x19, 0x19, 0x19));
            nvgFill(args.vg);
        }

        void drawLayer(const DrawArgs& args, int layer) override
        {
            if (layer == 1)
            {
                prepareFont(args);
                nvgFillColor(args.vg, fgColor);
                nvgText(args.vg, textPos.x, textPos.y, text.c_str(), nullptr);
            }
            Widget::drawLayer(args, layer);
        }

        void step() override
        {
            int channels = module ? module->currentChannelCount : 0;

            // The channel count display can actually show any number from 0..19,
            // even though we generally don't need numbers bigger than 16.
            if (channels >= 0 && channels <= 19)
                text = string::f("%d", channels);
            else
                text = "";
        }
    };


    using caption_button_base_t = VCVLightBezel<GrayModuleLightWidget>;
    struct SapphireCaptionButton : caption_button_base_t
    {
        std::string fontPath = asset::system("res/fonts/DejaVuSans.ttf");
        char caption[2]{};
        float dxText =  8.0;
        float dyText = 10.0;
        bool baseColorInitialized = false;

        void drawLayer(const DrawArgs& args, int layer) override
        {
            caption_button_base_t::drawLayer(args, layer);

            if (layer == 1)
            {
                if (caption[0])
                {
                    initBaseColor(SCHEME_WHITE);

                    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
                    if (font)
                    {
                        nvgFontSize(args.vg, 15);
                        nvgFontFaceId(args.vg, font->handle);
                        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 0xff));
                        float tx = (box.size.x - dxText) / 2;
                        float ty = (box.size.y + dyText) / 2;
                        nvgText(args.vg, tx, ty, caption, caption+1);
                    }
                }
            }
        }

        void setCaption(char c)
        {
            caption[0] = c;
        }

        void initBaseColor(NVGcolor color)
        {
            if (!baseColorInitialized)
            {
                baseColorInitialized = true;
                light->addBaseColor(color);
            }
        }
    };


    struct SapphireCaptionKnob : RoundLargeBlackKnob
    {
        std::string fontPath = asset::system("res/fonts/DejaVuSans.ttf");
        float dyText = 9.0;

        static float dxText(char c)
        {
            // I tried to use nvgTextBounds() to measure the dimenions of the rendered letters,
            // but I could never get results that looked good. So I have manually calibrated
            // the alignment by trial and error to look acceptable.
            switch (c)
            {
            case 'B':   return 8.4;
            case 'C':   return 9.0;
            case 'F':   return 8.3;
            default:    return 8.5;
            }
        }

        virtual char getCaption() const = 0;

        void drawLayer(const DrawArgs& args, int layer) override
        {
            RoundLargeBlackKnob::drawLayer(args, layer);
            if (layer == 1)
            {
                char caption[2];
                caption[0] = getCaption();
                caption[1] = '\0';
                if (caption[0])
                {
                    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
                    if (font)
                    {
                        nvgBeginPath(args.vg);
                        nvgStrokeColor(args.vg, SCHEME_BLACK);
                        nvgFillColor(args.vg, nvgRGBA(0x17, 0x17, 0x70, 0xc0));
                        const float dotRadius = 7.0;
                        nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, dotRadius);
                        nvgFill(args.vg);

                        nvgFontSize(args.vg, 15);
                        nvgFontFaceId(args.vg, font->handle);
                        nvgFillColor(args.vg, SCHEME_ORANGE);
                        float tx = (box.size.x - dxText(caption[0])) / 2;
                        float ty = (box.size.y + dyText) / 2;
                        nvgText(args.vg, tx, ty, caption, caption+1);
                    }
                }
            }
        }
    };


    using tiny_button_base_t = app::SvgSwitch;

    struct SapphireTinyButton : tiny_button_base_t
    {
        bool chainButtonClicks = true;

        virtual void action() {}
        virtual void restartChrono() {}

        void onButton(const ButtonEvent& e) override
        {
            if (module && e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS)
            {
                action();
                restartChrono();
            }

            if (chainButtonClicks)
                tiny_button_base_t::onButton(e);
        }
    };


    struct SapphireTinyToggleButton : SapphireTinyButton
    {
        explicit SapphireTinyToggleButton()
        {
            momentary = false;
        }
    };


    struct SapphireTinyActionButton : SapphireTinyButton
    {
        float blinkTimeSeconds = 0.02;
        Stopwatch stopwatch;

        explicit SapphireTinyActionButton()
        {
            momentary = true;
        }

        void step() override
        {
            SapphireTinyButton::step();

            if (stopwatch.elapsedSeconds() >= blinkTimeSeconds)
            {
                stopwatch.reset();
                if (ParamQuantity *quantity = getParamQuantity())
                    if (quantity->getValue() > 0)
                        quantity->setValue(0);
            }
        }

        void restartChrono() override
        {
            stopwatch.restart();
        }
    };


    struct SplashState
    {
        bool active = false;
        Stopwatch stopwatch;
        double durationSeconds = 0;
        double emphasis = 0;
        int rgb[3]{};
        float x1 = 0;

        void begin(int red, int green, int blue, double _durationSeconds = 0.25, double _emphasis = 0.5)
        {
            rgb[0] = red;
            rgb[1] = green;
            rgb[2] = blue;
            stopwatch.restart();
            durationSeconds = std::clamp<double>(_durationSeconds, 0.01, 10.0);
            emphasis = std::clamp<double>(_emphasis, 0, 1);
            active = true;
        }

        void end()
        {
            stopwatch.reset();
            active = false;
        }

        double remainingTime()
        {
            if (!active)
                return 0;

            double seconds = stopwatch.elapsedSeconds();
            return std::max<double>(0, durationSeconds - seconds);
        }
    };


    constexpr float DxRemoveGap = 0.3f;

    inline SvgOverlay* MakeSapphirePanel(const std::string& panelSvgFileName)
    {
        return new SvgOverlay(window::Svg::load(panelSvgFileName));
    }


    struct SapphireTooltip : Tooltip
    {
        bool isPositionLocked = false;
        Vec lockedPos{};

        void step() override
        {
            Tooltip::step();
            if (isPositionLocked)
            {
                box.pos = lockedPos;
            }
            else
            {
                lockedPos = box.pos;
                isPositionLocked = true;
            }
        }
    };


    struct SapphireWidget : ModuleWidget
    {
        const std::string modcode;
        const static NVGcolor neonColor;
        SplashState splash;

        SvgOverlay* outputStereoLabelLR = nullptr;
        SvgOverlay* outputStereoLabel2  = nullptr;

        SvgOverlay* inputStereoLabelLR = nullptr;
        SvgOverlay* inputStereoLabelL2 = nullptr;
        SvgOverlay* inputStereoLabelR2 = nullptr;

        explicit SapphireWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
            : modcode(moduleCode)
        {
            setPanel(MakeSapphirePanel(panelSvgFileName));
        }

        bool isNeonModeActive() const
        {
            const SapphireModule* sm = getSapphireModule();
            return sm && sm->neonMode;
        }

        static void ToggleAllNeonBorders();
        static void ToggleNeonBorder(SapphireModule* smod);

        void appendContextMenu(Menu* menu) override
        {
            if (SapphireModule* sm = getSapphireModule())
            {
                menu->addChild(new MenuSeparator);

                if (sm->includeNeonModeMenuItem)
                {
                    menu->addChild(createMenuItem(
                        "Toggle neon borders (this module only)",
                        "",
                        [=]() { ToggleNeonBorder(sm); }
                    ));
                }

                menu->addChild(createMenuItem(
                    "Toggle neon borders in all Sapphire modules",
                    "",
                    ToggleAllNeonBorders
                ));

                if (sm->dcRejectQuantity)
                    menu->addChild(new DcRejectSlider(sm->dcRejectQuantity));

                sm->addLimiterMenuItems(menu);
            }
        }

        void position(Widget* widget, const std::string& label)
        {
            ComponentLocation loc = FindComponent(modcode, label);
            Vec vec = mm2px(Vec{loc.cx, loc.cy});
            widget->box.pos = vec.minus(widget->box.size.div(2));
        }

        void addSapphireParam(ParamWidget* param, const std::string& label)
        {
            addParam(param);
            position(param, label);
        }

        void addSapphireInput(PortWidget* input, const std::string& label)
        {
            addInput(input);
            position(input, label);
        }

        template <typename port_t = SapphirePort>
        port_t* addSapphireInput(int inputId, const std::string& label)
        {
            port_t* port = createInputCentered<port_t>(Vec{}, module, inputId);
            addSapphireInput(port, label);
            return port;
        }

        void addSapphireOutput(PortWidget* output, const std::string& label)
        {
            addOutput(output);
            position(output, label);
        }

        template <typename port_t = SapphirePort>
        port_t* addSapphireOutput(int outputId, const std::string& label)
        {
            port_t *port = createOutputCentered<port_t>(Vec{}, module, outputId);
            port->module = dynamic_cast<SapphireModule*>(module);
            addSapphireOutput(port, label);
            return port;
        }

        void addStereoInputPorts(int leftPortId, int rightPortId, const std::string& prefix)
        {
            addSapphireInput(leftPortId,  prefix + "_left_input");
            addSapphireInput(rightPortId, prefix + "_right_input");
        }

        void addStereoOutputPorts(int leftPortId, int rightPortId, const std::string& prefix)
        {
            addSapphireOutput(leftPortId,  prefix + "_left_output");
            addSapphireOutput(rightPortId, prefix + "_right_output");
        }

        template <typename knob_t = RoundLargeBlackKnob>
        knob_t *addKnob(int paramId, const std::string& label)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, paramId);
            addSapphireParam(knob, label);
            return knob;
        }

        template <typename knob_t = RoundSmallBlackKnob>
        knob_t *addSmallKnob(int paramId, const std::string& label)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, paramId);
            addSapphireParam(knob, label);
            return knob;
        }

        template <typename knob_t>      // knob_t = OutputLimiterLargeKnob, OutputLimiterSmallKnob
        void installWarningLight(knob_t* knob)
        {
            if (SapphireModule* smod = getSapphireModule())
            {
                knob->sapphireModule = smod;
                smod->subscribe(knob);

                // Superimpose a warning light on the output level knob.
                // We turn the warning light on when the limiter is distoring the output.
                auto w = new WarningLightWidget(smod);
                w->box.pos = Vec{};
                w->box.size = knob->box.size;

                knob->warningLight = w;
                knob->addChild(w);
            }
        }

        template <typename knob_t>      // knob_t = OutputLimiterLargeKnob, OutputLimiterSmallKnob
        knob_t *addOutputLimiterKnob(int paramId, const std::string& label)
        {
            knob_t* knob = createParamCentered<knob_t>(Vec{}, module, paramId);
            installWarningLight(knob);
            addSapphireParam(knob, label);
            return knob;
        }

        SapphirePort* addFlippableOutputPort(int outputId, const std::string& label, SapphireModule* module)
        {
            SapphirePort* port = addSapphireOutput(outputId, label);
            port->allowsVoltageFlip = true;
            port->module = module;
            port->outputId = outputId;
            return port;
        }

        SapphireChannelDisplay* addSapphireChannelDisplay(const std::string& label)
        {
            SapphireChannelDisplay* display = createWidget<SapphireChannelDisplay>(Vec{});
            display->box.size = mm2px(Vec(8.197, 8.197));
            display->module = getSapphireModule();
            position(display, label);
            addChild(display);
            return display;
        }

        SapphireModule* getSapphireModule()
        {
            if (module == nullptr)
                return nullptr;

            SapphireModule* sapphireModule = dynamic_cast<SapphireModule*>(module);

            // Crash immediately to assist debugging if the module exists but is not a Sapphire module.
            // This means I likely started coding a new module but forgot to make it derive from SapphireModule.
            if (sapphireModule == nullptr)
                throw std::logic_error("Invalid usage of a non-Sapphire module.");

            return sapphireModule;
        }

        const SapphireModule* getSapphireModule() const
        {
            return const_cast<const SapphireModule*>(const_cast<SapphireWidget*>(this)->getSapphireModule());
        }

        template <typename knob_t = SapphireAttenuverterKnob>
        knob_t *addSapphireAttenuverter(int attenId, const std::string& label)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, attenId);
            SapphireModule* sapphireModule = getSapphireModule();

            if (sapphireModule)
            {
                // Restore the persisted sensitivity state we loaded from JSON (or false if none).
                knob->lowSensitivityMode = sapphireModule->lowSensitiveFlag(attenId);

                // Let the sensitivity toggler know this ID belongs to an attenuverter knob.
                sapphireModule->defineAttenuverterId(attenId);
            }

            // We need to put the knob on the screen whether this is a preview widget or a live module.
            addSapphireParam(knob, label);
            return knob;
        }

        void addSapphireControlGroup(const std::string& name, int knobId, int attenId, int cvInputId)
        {
            addKnob(knobId, name + "_knob");
            addSapphireAttenuverter(attenId, name + "_atten");
            addSapphireInput(cvInputId, name + "_cv");
        }

        template <typename knob_t = RoundSmallBlackKnob>
        knob_t* addSapphireFlatControlGroup(const std::string& prefix, int knobId, int attenId, int cvInputId)
        {
            knob_t* knob = addSmallKnob<knob_t>(knobId, prefix + "_knob");
            addSapphireAttenuverter(attenId, prefix + "_atten");
            addSapphireInput(cvInputId, prefix + "_cv");
            return knob;
        }

        template <typename knob_t = OutputLimiterSmallKnob>
        knob_t* addSapphireFlatControlGroupWithWarningLight(const std::string& prefix, int knobId, int attenId, int cvInputId)
        {
            knob_t* knob = addSapphireFlatControlGroup<knob_t>(prefix, knobId, attenId, cvInputId);
            installWarningLight(knob);
            return knob;
        }

        template <typename caption_button_t = SapphireCaptionButton, typename input_port_t = ToggleGroupInputPort>
        input_port_t* addToggleGroup(
            ToggleGroup* group,
            const std::string& prefix,
            int inputId,
            int buttonId,
            int lightId,
            char buttonLetter,
            float dxText,
            NVGcolor baseColor,
            bool momentary = false)
        {
            caption_button_t* button = createLightParamCentered<caption_button_t>(Vec{}, module, buttonId, lightId);
            button->momentary = momentary;
            button->dxText = dxText;
            button->setCaption(buttonLetter);
            button->initBaseColor(baseColor);

            addSapphireParam(button, prefix + "_button");
            input_port_t* port = addSapphireInput<input_port_t>(inputId, prefix + "_input");
            port->group = group;
            return port;
        }

        SvgOverlay* loadLabel(const char *svgFileName)
        {
            SvgOverlay* label = SvgOverlay::Load(svgFileName);
            label->setVisible(false);
            addChild(label);
            return label;
        }

        void loadOutputStereoLabels()
        {
            outputStereoLabel2  = loadLabel("res/stereo_out_2.svg");
            outputStereoLabelLR = loadLabel("res/stereo_out_lr.svg");
            outputStereoLabelLR->setVisible(true);
        }

        void loadInputStereoLabels()
        {
            inputStereoLabelL2 = loadLabel("res/stereo_in_l2.svg");
            inputStereoLabelR2 = loadLabel("res/stereo_in_r2.svg");
            inputStereoLabelLR = loadLabel("res/stereo_in_lr.svg");
        }

        bool isOutputStereoMergeEnabled()
        {
            const SapphireModule *sapphireModule = getSapphireModule();
            return sapphireModule && sapphireModule->enableStereoMerge;
        }

        InputStereoMode getInputStereoMode()
        {
            const SapphireModule *sapphireModule = getSapphireModule();
            if (sapphireModule == nullptr)
                return InputStereoMode::LeftRight;
            return sapphireModule->inputStereoMode;
        }

        void drawSplash(NVGcontext* vg, float x1);
        void drawLayer(const DrawArgs& args, int layer) override;

        void step() override
        {
            ModuleWidget::step();

            if (outputStereoLabel2 && outputStereoLabelLR)
            {
                const bool merge = isOutputStereoMergeEnabled();
                if (outputStereoLabel2->isVisible() != merge)
                {
                    outputStereoLabel2->setVisible(merge);
                    outputStereoLabelLR->setVisible(!merge);
                }
            }

            if (inputStereoLabelL2 && inputStereoLabelR2 && inputStereoLabelLR)
            {
                const InputStereoMode mode = getInputStereoMode();
                inputStereoLabelLR->setVisible(mode == InputStereoMode::LeftRight);
                inputStereoLabelL2->setVisible(mode == InputStereoMode::Left2);
                inputStereoLabelR2->setVisible(mode == InputStereoMode::Right2);
            }
        }

        bool isRightBorderHidden() const
        {
            const SapphireModule* smod = getSapphireModule();
            return smod && smod->hideRightBorder;
        }

        bool isLeftBorderHidden() const
        {
            const SapphireModule* smod = getSapphireModule();
            return smod && smod->hideLeftBorder;
        }

        void draw(const DrawArgs& args) override;
        void createTooltip(SapphireTooltip*& tooltip, const std::string& text);
        void destroyTooltip(SapphireTooltip*& tooltip);
        void updateTooltip(bool& flag, bool state, SapphireTooltip*& tooltip, const std::string& text);
    };

    SapphireModule* AddExpander(Model* model, ModuleWidget* parentModWidget, ExpanderDirection dir, bool clone = true);
    ModuleWidget* FindWidgetClosestOnRight(const ModuleWidget* origin, int hpDistanceLimit);

    struct PanelState
    {
        int64_t moduleId = -1;
        Vec oldPos{};
        Vec newPos{};

        explicit PanelState(ModuleWidget* mw)
            : moduleId(mw->module->id)
            , oldPos(mw->box.pos)
            {}
    };


    inline bool operator < (const PanelState& a, const PanelState& b)
    {
        if (a.oldPos.y != b.oldPos.y)
            return a.oldPos.y < b.oldPos.y;

        return a.oldPos.x < b.oldPos.x;
    }

    struct MoveExpanderAction : history::Action
    {
        std::vector<PanelState> movedPanels;

        explicit MoveExpanderAction(const std::vector<PanelState>& movedPanelList)
            : movedPanels(movedPanelList)
        {
            name = "move expander chain";
        }

        void undo() override
        {
            // Put any modules that were moved back where they came from.
            // We do this in ascending x-order, so that each module has
            // an empty space to land in.
            for (const PanelState& p : movedPanels)
            {
                ModuleWidget* widget = FindWidgetForId(p.moduleId);
                if (widget)
                    APP->scene->rack->requestModulePos(widget, p.oldPos);
            }
        }

        void redo() override
        {
            // Do everything in the same chronological that we did them originally.
            // Move modules out of the way, the same way the force-position thing does.
            // Move right-to-left by iterating in reverse order.
            for (auto p = movedPanels.rbegin(); p != movedPanels.rend(); ++p)
            {
                ModuleWidget* widget = FindWidgetForId(p->moduleId);
                if (widget)
                    APP->scene->rack->requestModulePos(widget, p->newPos);
            }
        }
    };

    struct AddExpanderAction : MoveExpanderAction
    {
        history::ModuleAdd addAction;

        explicit AddExpanderAction(
            Model* model,
            SapphireWidget* widget,
            const std::vector<PanelState>& movedPanelList
        )
            : MoveExpanderAction(movedPanelList)
        {
            name = "insert expander " + model->name;
            addAction.setModule(widget);
        }

        void undo() override
        {
            addAction.undo();
            MoveExpanderAction::undo();
        }

        void redo() override
        {
            MoveExpanderAction::redo();
            addAction.redo();
        }
    };

    std::vector<PanelState> SnapshotPanelPositions();
}
