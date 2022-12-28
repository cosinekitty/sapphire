#include "plugin.hpp"
#include "tubeunit_engine.hpp"

// Sapphire Tube Unit for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct TubeUnitModule : Module
{
    Sapphire::TubeUnitEngine engine[PORT_MAX_CHANNELS];

    enum ParamId
    {
        PARAMS_LEN
    };

    enum InputId
    {
        TUBE_VOCT_INPUT,
        FORMANT_VOCT_INPUT,
        VOICE_GATE_INPUT,
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

        configInput(TUBE_VOCT_INPUT, "Tube V/OCT");
        configInput(FORMANT_VOCT_INPUT, "Formant V/OCT");
        configInput(VOICE_GATE_INPUT, "Voice gate");

        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

        initialize();
    }

    void initialize()
    {
        for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
        {
            engine[c].initialize();
            engine[c].setRootFrequency(110.0f);
        }
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
        for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
            engine[c].setSampleRate(e.sampleRate);
    }

    void process(const ProcessArgs& args) override
    {
        // The current number of channels is determined
        // by the most polyphonic of the following input cables:
        // - Tube V/OCT
        // - Formant
        // - Gate
        // But still present one channel of output if there are no inputs.
        int tubeVoctChannels = inputs[TUBE_VOCT_INPUT].getChannels();
        int formantVoctChannels = inputs[FORMANT_VOCT_INPUT].getChannels();
        int voiceGateChannels = inputs[VOICE_GATE_INPUT].getChannels();
        int outputChannels = std::max({1, tubeVoctChannels, formantVoctChannels, voiceGateChannels});

        outputs[AUDIO_LEFT_OUTPUT ].setChannels(outputChannels);
        outputs[AUDIO_RIGHT_OUTPUT].setChannels(outputChannels);

        for (int c = 0; c < outputChannels; ++c)
        {
            float sample[2];
            engine[c].process(sample[0], sample[1]);

            // Normalize TubeUnitEngine's dimensionless [-1, 1] output to VCV Rack's 5.0V peak amplitude.
            sample[0] *= 5.0f;
            sample[1] *= 5.0f;

            outputs[AUDIO_LEFT_OUTPUT ].setVoltage(sample[0], c);
            outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sample[1], c);
        }
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

        // Input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.00, 20.00)), module, TubeUnitModule::TUBE_VOCT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.00, 40.00)), module, TubeUnitModule::FORMANT_VOCT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.00, 60.00)), module, TubeUnitModule::VOICE_GATE_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(40.46, 115.00)), module, TubeUnitModule::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(53.46, 115.00)), module, TubeUnitModule::AUDIO_RIGHT_OUTPUT));
    }
};

Model* modelTubeUnit = createModel<TubeUnitModule, TubeUnitWidget>("TubeUnit");
