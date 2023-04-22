#include "plugin.hpp"
#include "reloadable_widget.hpp"
#include "cantilever_engine.hpp"

// Sapphire Cantilever for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct CantileverModule : Module
{
    Sapphire::CantileverEngine engine{47};

    enum ParamId
    {
        BEND_KNOB_PARAM,
        TRIP_POSITION_PARAM,
        TRIP_LEVEL_PARAM,
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

    CantileverModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
        configParam(BEND_KNOB_PARAM, 0.0f, 1.0f, 0.5f, "Bend", "");
        configParam(TRIP_POSITION_PARAM, 0.0f, 1.0f, 0.5f, "Trip position x", "");
        configParam(TRIP_LEVEL_PARAM, 0.0f, 1.0f, 0.5f, "Trip level y", "");
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
        const float halflife = 5.0f;
        float sample[2];
        engine.setBend(params[BEND_KNOB_PARAM].getValue());
        engine.setTripX(params[TRIP_POSITION_PARAM].getValue());
        engine.setTripY(params[TRIP_LEVEL_PARAM].getValue());
        engine.process(args.sampleTime, halflife, sample);
        outputs[AUDIO_LEFT_OUTPUT ].setVoltage(5.0f * sample[0]);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(5.0f * sample[1]);
    }
};


struct CantileverWidget : ReloadableModuleWidget
{
    CantileverModule *cantileverModule;

    explicit CantileverWidget(CantileverModule *module)
        : ReloadableModuleWidget(asset::plugin(pluginInstance, "res/cantilever.svg"))
        , cantileverModule(module)
    {
        setModule(module);

        addSapphireOutput(CantileverModule::AUDIO_LEFT_OUTPUT,  "audio_left_output");
        addSapphireOutput(CantileverModule::AUDIO_RIGHT_OUTPUT, "audio_right_output");
        addKnob(CantileverModule::BEND_KNOB_PARAM, "bend_knob");
        addKnob(CantileverModule::TRIP_POSITION_PARAM, "tripx_knob");
        addKnob(CantileverModule::TRIP_LEVEL_PARAM, "tripy_knob");

        // Load the SVG and place all controls at their correct coordinates.
        reloadPanel();
    }
};


Model* modelCantilever = createModel<CantileverModule, CantileverWidget>("Cantilever");
