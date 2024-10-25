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

        void drawLayer(const DrawArgs& args, int layer) override
        {
            Trimpot::drawLayer(args, layer);

            if (layer == 1)
            {
                if ((lowSensitivityMode != nullptr) && *lowSensitivityMode)
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
            text = string::f("%d", channels);
        }
    };


    struct SapphireWidget : ModuleWidget
    {
        const std::string modcode;

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
            using namespace Sapphire;

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

        void addSapphireInput(int inputId, const std::string& svgId)
        {
            SapphirePort *port = createInputCentered<SapphirePort>(Vec{}, module, inputId);
            addSapphireInput(port, svgId);
        }

        void addSapphireOutput(PortWidget* output, const std::string& label)
        {
            addOutput(output);
            position(output, label);
        }

        SapphirePort* addSapphireOutput(int outputId, const std::string& svgId)
        {
            SapphirePort *port = createOutputCentered<SapphirePort>(Vec{}, module, outputId);
            addSapphireOutput(port, svgId);
            return port;
        }

        template <typename knob_t = RoundLargeBlackKnob>
        knob_t *addKnob(int paramId, const std::string& svgId)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, paramId);
            addSapphireParam(knob, svgId);
            return knob;
        }

        RoundSmallBlackKnob *addSmallKnob(int paramId, const std::string& svgId)
        {
            RoundSmallBlackKnob *knob = createParamCentered<RoundSmallBlackKnob>(Vec{}, module, paramId);
            addSapphireParam(knob, svgId);
            return knob;
        }

        SapphirePort* addFlippableOutputPort(int outputId, const std::string& svgId, SapphireModule* module)
        {
            SapphirePort* port = addSapphireOutput(outputId, svgId);
            port->allowsVoltageFlip = true;
            port->module = module;
            port->outputId = outputId;
            return port;
        }

        SapphireChannelDisplay* addSapphireChannelDisplay(const std::string& svgId)
        {
            SapphireChannelDisplay* display = createWidget<SapphireChannelDisplay>(Vec{});
            display->box.size = mm2px(Vec(8.197, 8.197));
            display->module = getSapphireModule();
            position(display, svgId);
            addChild(display);
            return display;
        }

        SapphireModule* getSapphireModule() const
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

        void addSapphireAttenuverter(int attenId, const std::string& svgId)
        {
            SapphireAttenuverterKnob *knob = createParamCentered<SapphireAttenuverterKnob>(Vec{}, module, attenId);
            SapphireModule* sapphireModule = getSapphireModule();

            if (sapphireModule != nullptr)
            {
                // Restore the persisted sensitivity state we loaded from JSON (or false if none).
                knob->lowSensitivityMode = sapphireModule->lowSensitiveFlag(attenId);

                // Let the sensitivity toggler know this ID belongs to an attenuverter knob.
                sapphireModule->defineAttenuverterId(attenId);
            }

            // We need to put the knob on the screen whether this is a preview widget or a live module.
            addSapphireParam(knob, svgId);
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
            SapphireModule *sapphireModule = getSapphireModule();
            return (sapphireModule != nullptr) && sapphireModule->enableStereoMerge;
        }

        InputStereoMode getInputStereoMode()
        {
            SapphireModule *sapphireModule = getSapphireModule();
            if (sapphireModule == nullptr)
                return InputStereoMode::LeftRight;
            return sapphireModule->inputStereoMode;
        }

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
}
