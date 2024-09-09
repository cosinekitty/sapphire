/*
    reloadable_widget.hpp  -  Don Cross <cosinekitty@gmail.com>
    https://github.com/cosinekitty/sapphire

    An extension to the VCV Rack SDK which facilitates making
    a module where both the artwork and port/control positions are
    defined by an SVG file. The SVG file can be modified while
    the module is running and dynamically reloaded.
    This allows for a much faster development cycle where
    the design can be updated iteratively without restarting
    VCV Rack or rebuilding your C++ code.
*/

#pragma once
#include <rack.hpp>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sapphire_panel.hpp"


namespace rack
{
    struct ReloadableModuleWidget : ModuleWidget
    {
        const std::string modcode;

        explicit ReloadableModuleWidget(const std::string& moduleCode, const std::string& svgFileName)
            : modcode(moduleCode)
        {
            app::SvgPanel* svgPanel = createPanel(svgFileName);
            setPanel(svgPanel);
        }

        void position(Widget* widget, const std::string& label)
        {
            using namespace Sapphire;

            ComponentLocation loc = TheComponentPlacer.find(modcode, label);
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
    };
}
