#include "plugin.hpp"
#include "reloadable_widget.hpp"
#include "water_engine.hpp"

// Sapphire WaterPool for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct WaterPoolModule : Module
{
    Sapphire::WaterEngine engine;

    enum ParamId
    {
        PROPAGATION_PARAM,
        HALFLIFE_PARAM,
        RADIUS_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        AUDIO_LEFT_INPUT,
        AUDIO_RIGHT_INPUT,
        INPUTS_LEN
    };

    enum OutputId
    {
        AUDIO_LEFT_OUTPUT,
        AUDIO_RIGHT_OUTPUT,
        AGITATOR_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId
    {
        LIGHTS_LEN
    };

    WaterPoolModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(PROPAGATION_PARAM, 4.0f, 8.9f, 7.0f, "Propagation");
        configParam(HALFLIFE_PARAM, -3.0f, +1.0f, -1.0f, "Decay");
        configParam(RADIUS_PARAM, -5.0f, -1.0f, -3.0f, "Radius");
        configInput(AUDIO_LEFT_INPUT, "Left audio");
        configInput(AUDIO_RIGHT_INPUT, "Right audio");
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
        configOutput(AGITATOR_OUTPUT, "Agitator");
        configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
        configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);
        initialize();
    }

    void initialize()
    {
        engine.initialize();
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    void process(const ProcessArgs& args) override
    {
        using namespace Sapphire;

        float leftIn = inputs[AUDIO_LEFT_INPUT].getVoltageSum();
        float rightIn = inputs[AUDIO_RIGHT_INPUT].getVoltageSum();
        engine.setPropagation(params[PROPAGATION_PARAM].getValue());
        engine.setHalfLife(params[HALFLIFE_PARAM].getValue());
        engine.setDetectorRadius(params[RADIUS_PARAM].getValue());
        float sample[2];
        engine.process(args.sampleTime, sample[0], sample[1], leftIn, rightIn);
        outputs[AUDIO_LEFT_OUTPUT].setVoltage(sample[0]);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sample[1]);
        outputs[AGITATOR_OUTPUT].setVoltage(engine.getAgitator());
    }
};


struct WaterPoolWidget : ReloadableModuleWidget
{
    WaterPoolModule *waterPoolModule;

    explicit WaterPoolWidget(WaterPoolModule* module)
        : ReloadableModuleWidget(asset::plugin(pluginInstance, "res/waterpool.svg"))
        , waterPoolModule(module)
    {
        setModule(module);

        addKnob(WaterPoolModule::PROPAGATION_PARAM, "propagation_knob");
        addKnob(WaterPoolModule::HALFLIFE_PARAM, "decay_knob");
        addKnob(WaterPoolModule::RADIUS_PARAM, "radius_knob");

        // Audio input Jacks
        addSapphireInput(WaterPoolModule::AUDIO_LEFT_INPUT, "audio_left_input");
        addSapphireInput(WaterPoolModule::AUDIO_RIGHT_INPUT, "audio_right_input");

        // Audio output jacks
        addSapphireOutput(WaterPoolModule::AUDIO_LEFT_OUTPUT, "audio_left_output");
        addSapphireOutput(WaterPoolModule::AUDIO_RIGHT_OUTPUT, "audio_right_output");
        addSapphireOutput(WaterPoolModule::AGITATOR_OUTPUT, "agitator_output");

        // Load the SVG and place all controls at their correct coordinates.
        reloadPanel();
    }
};


Model* modelWaterPool = createModel<WaterPoolModule, WaterPoolWidget>("WaterPool");
