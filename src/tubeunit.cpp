#include "plugin.hpp"
#include "tubeunit_engine.hpp"

// Sapphire Tube Unit for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct TubeUnitModule : Module
{
    Sapphire::TubeUnitEngine engine;

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

    TubeUnitModule()
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

    json_t* dataToJson() override
    {
        json_t* root = json_object();
        return root;
    }

    void dataFromJson(json_t* root) override
    {
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override
    {
    }

    void process(const ProcessArgs& args) override
    {
        float sample[2];

        engine.process(args.sampleRate, sample[0], sample[1]);

        // Normalize TubeUnitEngine's dimensionless [-1, 1] output to VCV Rack's 5.0V peak amplitude.
        sample[0] *= 5.0f;
        sample[1] *= 5.0f;

        outputs[AUDIO_LEFT_OUTPUT].setVoltage(sample[0]);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sample[1]);
    }
};


struct TubeUnitWidget : ModuleWidget
{
    TubeUnitModule *tubeUnitModule;

    TubeUnitWidget(TubeUnitModule* module)
        : tubeUnitModule(module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/tubeunit.svg")));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(40.46, 115.00)), module, TubeUnitModule::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(53.46, 115.00)), module, TubeUnitModule::AUDIO_RIGHT_OUTPUT));
    }
};

Model* modelTubeUnit = createModel<TubeUnitModule, TubeUnitWidget>("TubeUnit");
