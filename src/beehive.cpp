/*
    beehive.cpp  -  Don Cross

    This is a dummy plugin for experimenting with a "hotload" panel design workflow.
*/

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
    std::string svgFileName;
    app::SvgPanel *svgPanel = nullptr;

    BeehiveWidget(BeehiveModule *module)
    {
        setModule(module);

        svgFileName = asset::plugin(pluginInstance, "res/beehive.svg");
        svgPanel = createPanel(svgFileName);
        setPanel(svgPanel);
    }
};


Model* modelBeehive = createModel<BeehiveModule, BeehiveWidget>("Beehive");
