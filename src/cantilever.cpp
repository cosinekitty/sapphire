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
        STRETCH_KNOB_PARAM,
        BEND_KNOB_PARAM,
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
        configParam(STRETCH_KNOB_PARAM, 0.0f, 1.0f, 0.5f, "Stretch", "");
        configParam(BEND_KNOB_PARAM, 0.0f, 1.0f, 0.5f, "Bend", "");
        initialize();
    }

    void initialize()
    {
        engine.initialize();
        engine.rotateRod(engine.nrods-1, 5.0f);
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
        engine.setStretch(params[STRETCH_KNOB_PARAM].getValue());
        engine.setBend(params[BEND_KNOB_PARAM].getValue());
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
        addKnob(CantileverModule::STRETCH_KNOB_PARAM, "stretch_knob");
        addKnob(CantileverModule::BEND_KNOB_PARAM, "bend_knob");

        // Load the SVG and place all controls at their correct coordinates.
        reloadPanel();
    }
};


Model* modelCantilever = createModel<CantileverModule, CantileverWidget>("Cantilever");
