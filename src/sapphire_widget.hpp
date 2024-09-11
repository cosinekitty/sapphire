/*
    sapphire_widget.hpp  -  Don Cross <cosinekitty@gmail.com>
    https://github.com/cosinekitty/sapphire

    Custom extensions to my ReloadableModuleWidget class.
    These are particular to my Sapphire plugin,
    while ReloadableModuleWidget is intended to be reusable
    by other VCV Rack module developers.
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


    struct SapphireWidget : ModuleWidget
    {
        const std::string modcode;

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

        void addReloadableParam(ParamWidget* param, const std::string& label)
        {
            addParam(param);
            position(param, label);
        }

        void addReloadableInput(PortWidget* input, const std::string& label)
        {
            addInput(input);
            position(input, label);
        }

        void addReloadableOutput(PortWidget* output, const std::string& label)
        {
            addOutput(output);
            position(output, label);
        }

        template <typename knob_t = RoundLargeBlackKnob>
        knob_t *addKnob(int paramId, const std::string& svgId)
        {
            knob_t *knob = createParamCentered<knob_t>(Vec{}, module, paramId);
            addReloadableParam(knob, svgId);
            return knob;
        }

        RoundSmallBlackKnob *addSmallKnob(int paramId, const std::string& svgId)
        {
            RoundSmallBlackKnob *knob = createParamCentered<RoundSmallBlackKnob>(Vec{}, module, paramId);
            addReloadableParam(knob, svgId);
            return knob;
        }

        void addSapphireInput(int inputId, const std::string& svgId)
        {
            SapphirePort *port = createInputCentered<SapphirePort>(Vec{}, module, inputId);
            addReloadableInput(port, svgId);
        }

        SapphirePort* addSapphireOutput(int outputId, const std::string& svgId)
        {
            SapphirePort *port = createOutputCentered<SapphirePort>(Vec{}, module, outputId);
            addReloadableOutput(port, svgId);
            return port;
        }

        SapphirePort* addFlippableOutputPort(int outputId, const std::string& svgId, SapphireModule* module)
        {
            SapphirePort* port = addSapphireOutput(outputId, svgId);
            port->allowsVoltageFlip = true;
            port->module = module;
            port->outputId = outputId;
            return port;
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
            addReloadableParam(knob, svgId);
        }

        void addSapphireControlGroup(const std::string& name, int knobId, int attenId, int cvInputId)
        {
            addKnob(knobId, name + "_knob");
            addSapphireAttenuverter(attenId, name + "_atten");
            addSapphireInput(cvInputId, name + "_cv");
        }

        void addSapphireFlatControlGroup(const std::string& prefix, int knobId, int attenId, int cvInputId)
        {
            addSmallKnob(knobId, prefix + "_knob");
            addSapphireAttenuverter(attenId, prefix + "_atten");
            addSapphireInput(cvInputId, prefix + "_cv");
        }
    };
}
