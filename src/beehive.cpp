/*
    beehive.cpp  -  Don Cross

    This is a dummy plugin for experimenting with a "hotload" panel design workflow.
*/

#include "plugin.hpp"

struct BeehiveModule : Module
{
    enum ParamId
    {
        FREQUENCY_PARAM,
        RESONANCE_PARAM,
        DECAY_PARAM,
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

        configParam(FREQUENCY_PARAM, 0.0f, 1.0f, 0.5f, "Frequency");
        configParam(RESONANCE_PARAM, 0.0f, 1.0f, 0.5f, "Resonance");
        configParam(DECAY_PARAM, 0.0f, 1.0f, 0.5f, "Decay");

        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

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


struct BeehiveWidget : ReloadableModuleWidget
{
    BeehiveWidget(BeehiveModule *module)
        : ReloadableModuleWidget(asset::plugin(pluginInstance, "res/beehive.svg"))
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

        createLargeKnob(BeehiveModule::FREQUENCY_PARAM, "beehive_frequency_knob");
        createLargeKnob(BeehiveModule::RESONANCE_PARAM, "beehive_resonance_knob");
        createLargeKnob(BeehiveModule::DECAY_PARAM,     "beehive_decay_knob");

        createOutputPort(BeehiveModule::AUDIO_LEFT_OUTPUT,  "beehive_output_left");
        createOutputPort(BeehiveModule::AUDIO_RIGHT_OUTPUT, "beehive_output_right");
    }

    void createLargeKnob(BeehiveModule::ParamId paramId, std::string svgId)
    {
        RoundLargeBlackKnob *knob = createParam<RoundLargeBlackKnob>(Vec{}, module, paramId);
        addReloadableParam(knob, svgId);
    }

    void createOutputPort(BeehiveModule::OutputId outputId, std::string svgId)
    {
        SapphirePort *port = createOutput<SapphirePort>(Vec{}, module, outputId);
        addReloadableOutput(port, svgId);
    }
};


Model* modelBeehive = createModel<BeehiveModule, BeehiveWidget>("Beehive");
