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

namespace rack
{
    struct ReloadableModuleWidget : ModuleWidget
    {
        std::string svgFileName;
        std::map<std::string, Widget*> svgWidgetMap;
        app::SvgPanel* svgPanel = nullptr;
        bool isReloadEnabled = false;
        bool isPollingEnabled = false;
        double prevPollTime = 0.0;
        struct stat prevStatBuf {};

        explicit ReloadableModuleWidget(const std::string& panelSvgFileName)
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

        void addReloadableParam(ParamWidget* param, const std::string& svgid)
        {
            addParam(param);
            svgWidgetMap[svgid] = param;
        }

        void addReloadableInput(PortWidget* input, const std::string& svgid)
        {
            addInput(input);
            svgWidgetMap[svgid] = input;
        }

        void addReloadableOutput(PortWidget* output, const std::string& svgid)
        {
            addOutput(output);
            svgWidgetMap[svgid] = output;
        }

        void appendContextMenu(Menu *menu) override
        {
            if (isReloadEnabled)
            {
                menu->addChild(new MenuSeparator);
                menu->addChild(createMenuItem("Reload panel now", "F5", [this]{ reloadPanel(); }));
                menu->addChild(createBoolPtrMenuItem<bool>("Poll SVG for reload", "", &isPollingEnabled));
            }
        }

        void onHoverKey(const HoverKeyEvent& e) override
        {
            if (isReloadEnabled && (e.key == GLFW_KEY_F5))
            {
                reloadPanel();
                e.consume(this);
                return;
            }
            ModuleWidget::onHoverKey(e);
        }

        void step() override
        {
            ModuleWidget::step();
            if (isPollingEnabled)
            {
                // Check the SVG file's last-modification-time once per second.
                double now = system::getTime();
                double elapsed = now - prevPollTime;
                if (elapsed >= 1.0)
                {
                    prevPollTime = now;
                    struct stat statBuf;
                    if (0 == stat(svgFileName.c_str(), &statBuf))
                    {
                        // Has the SVG file's modification time changed?
                        // For maximum source portability, check the POSIX-required seconds field only.
                        // We aren't polling more than once per second, and realistically,
                        // the SVG file isn't changing more than once per second either.
                        if (0 != memcmp(&statBuf.st_mtime, &prevStatBuf.st_mtime, sizeof(statBuf.st_mtime)))
                        {
                            prevStatBuf = statBuf;
                            reloadPanel();
                        }
                    }
                }
            }
        }
    };
}
