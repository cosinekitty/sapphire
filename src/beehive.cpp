/*
    beehive.cpp  -  Don Cross

    This is a dummy plugin for experimenting with a "hotload" panel design workflow.
*/

#include <map>
#include "plugin.hpp"

struct BeehiveModule : Module
{
    enum ParamId
    {
        PARAMS_LEN
    };

    enum InputId
    {
        INPUTS_LEN
    };

    enum OutputId
    {
        AUDIO_LEFT_OUTPUT,
        AUDIO_RIGHT_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId
    {
        LIGHTS_LEN
    };

    BeehiveModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        initialize();
    }

    void initialize()
    {
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    void process(const ProcessArgs& args) override
    {
    }
};


struct BeehiveWidget : ModuleWidget
{
    const std::string svgFileName = asset::plugin(pluginInstance, "res/beehive.svg");
    std::map<std::string, Widget*> lookup;
    app::SvgPanel* svgPanel = nullptr;

    BeehiveWidget(BeehiveModule *module)
    {
        setModule(module);
        createComponents();
        reloadPanel();
    }

    void createComponents()
    {
        // Create all the controls: input ports, output ports, and parameters.
        // But don't worry about where they go! Just put them all at (0, 0).
        // The reloadPanel function will move everything to the correct location
        // based on information from the SVG file.
        createOutputPort(BeehiveModule::AUDIO_LEFT_OUTPUT,  "beehive_output_left");
        createOutputPort(BeehiveModule::AUDIO_RIGHT_OUTPUT, "beehive_output_right");
    }

    void createOutputPort(BeehiveModule::OutputId id, std::string name)
    {
        SapphirePort *port = createOutput<SapphirePort>(Vec{}, module, id);
        lookup[name] = port;
        addOutput(port);
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
                auto search = lookup.find(shape->id);
                if (search != lookup.end())
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

    void appendContextMenu(Menu *menu) override
    {
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reload panel", "", [this]{ reloadPanel(); }));
    }
};


Model* modelBeehive = createModel<BeehiveModule, BeehiveWidget>("Beehive");
