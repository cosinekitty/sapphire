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

namespace rack
{
    struct SapphireReloadableModuleWidget : ReloadableModuleWidget
    {
        explicit SapphireReloadableModuleWidget(const std::string& panelSvgFileName)
            : ReloadableModuleWidget(panelSvgFileName)
            {}

        void addSapphireInput(int paramId, const char *svgId)
        {
            SapphirePort *port = createInputCentered<SapphirePort>(Vec{}, module, paramId);
            addReloadableInput(port, svgId);
        }

        void addSapphireOutput(int paramId, const char *svgId)
        {
            SapphirePort *port = createOutputCentered<SapphirePort>(Vec{}, module, paramId);
            addReloadableOutput(port, svgId);
        }
    };
}
