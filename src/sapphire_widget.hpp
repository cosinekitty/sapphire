/*
    sapphire_widget.hpp  -  Don Cross <cosinekitty@gmail.com>
    https://github.com/cosinekitty/sapphire
*/

#pragma once
#include "sapphire_panel.hpp"

namespace Sapphire
{
    struct SapphireAttenuverterKnob : Trimpot
    {
        bool* lowSensitivityMode = nullptr;

        void appendContextMenu(ui::Menu* menu) override
        {
            Trimpot::appendContextMenu(menu);
            if (lowSensitivityMode != nullptr)
                menu->addChild(createBoolPtrMenuItem<bool>("Low sensitivity", "", lowSensitivityMode));
        }

        bool isLowSensitive() const
        {
            return (lowSensitivityMode != nullptr) && *lowSensitivityMode;
        }

        void setLowSensitive(bool s)
        {
            if (lowSensitivityMode != nullptr)
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


    struct SplashState
    {
        bool active = false;
        Stopwatch stopwatch;
        double durationSeconds = 0;
        double emphasis = 0;

        void begin(double _durationSeconds = 0.25, double _emphasis = 0.5)
        {
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


    struct SapphireWidget : ModuleWidget
    {
        const std::string modcode;
        SplashState splash;

        SvgOverlay* outputStereoLabelLR = nullptr;
        SvgOverlay* outputStereoLabel2  = nullptr;

        SvgOverlay* inputStereoLabelLR = nullptr;
        SvgOverlay* inputStereoLabelL2 = nullptr;
        SvgOverlay* inputStereoLabelR2 = nullptr;

        explicit SapphireWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
            : modcode(moduleCode)
        {
            app::SvgPanel* svgPanel = createPanel(panelSvgFileName);
            setPanel(svgPanel);
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

        void addSapphireInput(int inputId, const std::string& label)
        {
            SapphirePort *port = createInputCentered<SapphirePort>(Vec{}, module, inputId);
            addSapphireInput(port, label);
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
            addSapphireOutput(port, label);
            return port;
        }

        template <typename knob_t = RoundLargeBlackKnob>
        knob_t *addKnob(int paramId, const std::string& label)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, paramId);
            addSapphireParam(knob, label);
            return knob;
        }

        RoundSmallBlackKnob *addSmallKnob(int paramId, const std::string& label)
        {
            RoundSmallBlackKnob *knob = createParamCentered<RoundSmallBlackKnob>(Vec{}, module, paramId);
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

        template <typename knob_t = SapphireAttenuverterKnob>
        knob_t *addSapphireAttenuverter(int attenId, const std::string& label)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, attenId);
            SapphireModule* sapphireModule = getSapphireModule();

            if (sapphireModule != nullptr)
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

        RoundSmallBlackKnob* addSapphireFlatControlGroup(const std::string& prefix, int knobId, int attenId, int cvInputId)
        {
            RoundSmallBlackKnob* knob = addSmallKnob(knobId, prefix + "_knob");
            addSapphireAttenuverter(attenId, prefix + "_atten");
            addSapphireInput(cvInputId, prefix + "_cv");
            return knob;
        }

        void addToggleGroup(
            const std::string& prefix,
            int inputId,
            int buttonId,
            int lightId,
            char buttonLetter,
            float dxText,
            NVGcolor baseColor)
        {
            SapphireCaptionButton* button = createLightParamCentered<SapphireCaptionButton>(Vec{}, module, buttonId, lightId);
            button->momentary = false;
            button->latch = true;
            button->dxText = dxText;
            button->setCaption(buttonLetter);
            button->initBaseColor(baseColor);

            addSapphireParam(button, prefix + "_button");
            addSapphireInput(inputId, prefix + "_input");
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
            return (sapphireModule != nullptr) && sapphireModule->enableStereoMerge;
        }

        InputStereoMode getInputStereoMode()
        {
            const SapphireModule *sapphireModule = getSapphireModule();
            if (sapphireModule == nullptr)
                return InputStereoMode::LeftRight;
            return sapphireModule->inputStereoMode;
        }

        void drawSplash(NVGcontext* vg);
        void drawLayer(const DrawArgs& args, int layer) override;
        void eraseBorder(NVGcontext* vg, int side);
        void updateBorders(NVGcontext* vg);

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
    };

    SapphireModule* AddExpander(Model* model, ModuleWidget* parentModWidget, ExpanderDirection dir);
}
