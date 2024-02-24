/*
    sapphire_widget.hpp  -  Don Cross <cosinekitty@gmail.com>
    https://github.com/cosinekitty/sapphire

    Custom extensions to my ReloadableModuleWidget class.
    These are particular to my Sapphire plugin,
    while ReloadableModuleWidget is intended to be reusable
    by other VCV Rack module developers.
*/

#pragma once
#include "reloadable_widget.hpp"

namespace Sapphire
{
    struct SapphireAttenuverterKnob : Trimpot
    {
        uint8_t* lowSensitivityMode = nullptr;

        void appendContextMenu(ui::Menu* menu) override
        {
            Trimpot::appendContextMenu(menu);
            if (lowSensitivityMode != nullptr)
                menu->addChild(createBoolPtrMenuItem<uint8_t>("Reduce knob sensitivity (10X)", "", lowSensitivityMode));
        }
    };

    struct SapphireReloadableModuleWidget : ReloadableModuleWidget
    {
        explicit SapphireReloadableModuleWidget(const std::string& panelSvgFileName)
            : ReloadableModuleWidget(panelSvgFileName)
            {}

        void addSapphireInput(int paramId, const std::string& svgId)
        {
            SapphirePort *port = createInputCentered<SapphirePort>(Vec{}, module, paramId);
            addReloadableInput(port, svgId);
        }

        void addSapphireOutput(int paramId, const std::string& svgId)
        {
            SapphirePort *port = createOutputCentered<SapphirePort>(Vec{}, module, paramId);
            addReloadableOutput(port, svgId);
        }

        void addSapphireAttenuverter(int paramId, const char *svgId)
        {
            SapphireAttenuverterKnob *knob = createParamCentered<SapphireAttenuverterKnob>(Vec{}, module, paramId);
            SapphireModule* sapphireModule = dynamic_cast<SapphireModule*>(module);

            // If module is null, it means this widget was created for display purposes only.
            // If module does not derive from SapphireModule, we don't know how to access the boolean flag we need.
            // In either of those unwanted cases, sapphireModule will be null.
            if (sapphireModule != nullptr)
            {
                // The knob has a context menu. Inside that menu, we provide a
                // toggle option for low sensitivity mode.
                // When this widget is connected to a module, and that module
                // derives from SapphireModule, we have access to the location
                // of the low-sensitivity flag. We can toggle it any time and the attenuverter
                // behavior (and its knob's appearance) will change immediately.
                knob->lowSensitivityMode = sapphireModule->lowSensitiveFlag(paramId);
            }

            // We need to put the knob on the screen whether this is a preview widget or a live module.
            addReloadableParam(knob, svgId);
        }
    };
}
