/*
    reloadable_module.hpp  -  Don Cross <cosinekitty@gmail.com>
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

namespace rack
{
    struct ReloadableModuleWidget : ModuleWidget
    {
        std::string svgFileName;
        std::map<std::string, Widget*> svgWidgetMap;
        app::SvgPanel* svgPanel = nullptr;

        ReloadableModuleWidget(std::string panelSvgFileName)
            : svgFileName(panelSvgFileName)
        {
        }

        void reloadPanel()
        {
            if (svgPanel == nullptr)
            {
                // Load and parse the SVG file for the first time.
                svgPanel = createPanel(svgFileName);
                setPanel(svgPanel);
            }
            else
            {
                // Once loaded, VCV Rack caches the panel internally.
                // We have to force it to reload the file.
                try
                {
                    svgPanel->svg->loadFile(svgFileName);
                }
                catch (Exception& e)
                {
                    WARN("Cannot reload panel from %s: %s", svgFileName.c_str(), e.what());
                }
            }

            // Find shapes whose SVG identifier matches one of our control names.
            // Use coordinates from the SVG object to set the position of the matching control.
            if (svgPanel && svgPanel->svg && svgPanel->svg->handle)
            {
                for (NSVGshape* shape = svgPanel->svg->handle->shapes; shape != nullptr; shape = shape->next)
                {
                    auto search = svgWidgetMap.find(shape->id);
                    if (search != svgWidgetMap.end())
                        reposition(search->second, shape);
                }
            }

            if (svgPanel && svgPanel->fb)
            {
                // Mark the SVG frame buffer as dirty, so it forces a redraw.
                svgPanel->fb->dirty = true;
            }
        }

        void reposition(Widget* widget, NSVGshape* shape)
        {
            float x = (shape->bounds[0] + shape->bounds[2]) / 2;
            float y = (shape->bounds[1] + shape->bounds[3]) / 2;
            widget->box.pos = Vec{x, y}.minus(widget->box.size.div(2));
        }

        void addReloadableParam(ParamWidget* param, std::string svgid)
        {
            addParam(param);
            svgWidgetMap[svgid] = param;
        }

        void addReloadableInput(PortWidget* input, std::string svgid)
        {
            addInput(input);
            svgWidgetMap[svgid] = input;
        }

        void addReloadableOutput(PortWidget* output, std::string svgid)
        {
            addOutput(output);
            svgWidgetMap[svgid] = output;
        }
    };
}
