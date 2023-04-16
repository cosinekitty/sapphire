#include "plugin.hpp"
#include "reloadable_widget.hpp"
#include "cantilever_engine.hpp"

// Sapphire Cantilever for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct CantileverModule : Module
{
    Sapphire::CantileverEngine engine;

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

    CantileverModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
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
        engine.process(args.sampleTime, halflife, sample);
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

        // Load the SVG and place all controls at their correct coordinates.
        reloadPanel();
    }
};


Model* modelCantilever = createModel<CantileverModule, CantileverWidget>("Cantilever");
