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
#include <unistd.h>

namespace rack
{
    struct ReloadableModuleWidget : ModuleWidget
    {
        std::string svgFileName;
        std::map<std::string, Widget*> svgWidgetMap;
        app::SvgPanel* svgPanel = nullptr;
        bool isReloadEnabled = false;

        ReloadableModuleWidget(std::string panelSvgFileName)
            : svgFileName(panelSvgFileName)
        {
            std::string flagFileName = panelSvgFileName + ".reload";
            isReloadEnabled = (0 == access(flagFileName.c_str(), F_OK));
        }

        void reloadPanel()
        {
            if (svgPanel == nullptr)
            {
                // Load and parse the SVG file for the first time.
                svgPanel = createPanel(svgFileName);
                setPanel(svgPanel);
            }
            else if (svgPanel->svg != nullptr)
            {
                // Once loaded, VCV Rack caches the panel internally.
                // We have to force it to reload and reparse the SVG file.
                // Attempt to create a new SVG handle before replacing the one
                // that exists. This way, in case the file is missing or corrupt,
                // we don't lose the existing panel, nor do we risk crashing VCV Rack.
                // This is quite likely during iterative development, which is why
                // this code exists in the first place!
                NSVGimage *replacement = nsvgParseFromFile(svgFileName.c_str(), "px", SVG_DPI);
                if (replacement == nullptr)
                {
                    // Leave the existing panel in place, and log why it didn't change.
                    WARN("Cannot load/parse SVG file [%s]", svgFileName.c_str());
                }
                else
                {
                    // Successful reload. Destroy the old SVG and replace it with the new one.
                    if (svgPanel->svg->handle)
                        nsvgDelete(svgPanel->svg->handle);

                    svgPanel->svg->handle = replacement;
                }
            }
            else
            {
                // This should never happen. If it does, there is a bug I need to fix.
                WARN("Weird! Somehow we lost our SVG panel.");
            }

            if (svgPanel && svgPanel->svg && svgPanel->svg->handle)
            {
                // Find shapes whose SVG identifier matches one of our control names.
                // Use coordinates from the SVG object to set the position of the matching control.
                for (NSVGshape* shape = svgPanel->svg->handle->shapes; shape != nullptr; shape = shape->next)
                {
                    auto search = svgWidgetMap.find(shape->id);
                    if (search != svgWidgetMap.end())
                        reposition(search->second, shape);
                }

                if (svgPanel->fb)
                {
                    // Mark the SVG frame buffer as dirty to force redrawing the panel.
                    svgPanel->fb->dirty = true;
                }
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

        void appendContextMenu(Menu *menu) override
        {
            if (isReloadEnabled)
            {
                menu->addChild(new MenuSeparator);
                menu->addChild(createMenuItem("Reload panel", "", [this]{ reloadPanel(); }));
            }
        }
    };
}
