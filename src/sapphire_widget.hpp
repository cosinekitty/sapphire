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
        bool* lowSensitivityMode = nullptr;

        void appendContextMenu(ui::Menu* menu) override
        {
            Trimpot::appendContextMenu(menu);
            if (lowSensitivityMode != nullptr)
                menu->addChild(createBoolPtrMenuItem<bool>("Low sensitivity", "", lowSensitivityMode));
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
                knob->lowSensitivityMode = sapphireModule->lowSensitiveFlag(attenId);

            // We need to put the knob on the screen whether this is a preview widget or a live module.
            addReloadableParam(knob, svgId);
        }
    };
}
