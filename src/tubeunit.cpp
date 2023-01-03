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
        AIRFLOW_INPUT,
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
        configInput(AIRFLOW_INPUT, "Airflow");

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
        using namespace Sapphire;

        int tubeVoctChannels = inputs[TUBE_VOCT_INPUT].getChannels();
        int airflowChannels = inputs[AIRFLOW_INPUT].getChannels();

        // The current number of channels is determined
        // by the most polyphonic of the following input cables:
        // - Tube V/OCT
        // - Formant V/OCT
        // - Voice Gate
        // But still present one channel of output if there are no inputs.
        int outputChannels = std::max({1, tubeVoctChannels, airflowChannels});

        outputs[AUDIO_LEFT_OUTPUT ].setChannels(outputChannels);
        outputs[AUDIO_RIGHT_OUTPUT].setChannels(outputChannels);

        // Any of the inputs could have any number of channels 0..16.
        // We use simple and consistent rules to handle all possible cases:
        //
        // 1. Set the respective inputs to a default value.
        // 2. Each time we move to a new channel, if there is a corresponding input channel,
        //    update the input value.
        // 3. Otherwise keep the most recent value for the remaining channels.
        //
        // In other words, whichever input has the most channels selects the output channel count.
        // Other inputs have their final supplied value (or default value if none)
        // "normalled" to the remaining channels.

        float tubeFreqHz = TubeUnitDefaultRootFrequencyHz;
        float airflow = 0.0f;

        for (int c = 0; c < outputChannels; ++c)
        {
            if (c < tubeVoctChannels)
                tubeFreqHz = FrequencyFromVoct(inputs[TUBE_VOCT_INPUT].getVoltage(c), TubeUnitDefaultRootFrequencyHz);

            if (c < airflowChannels)
                airflow = inputs[AIRFLOW_INPUT].getVoltage(c);

            engine[c].setAirflow(airflow / 5.0);
            engine[c].setRootFrequency(tubeFreqHz);

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
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.00, 40.00)), module, TubeUnitModule::AIRFLOW_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(40.46, 115.00)), module, TubeUnitModule::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(53.46, 115.00)), module, TubeUnitModule::AUDIO_RIGHT_OUTPUT));
    }
};

Model* modelTubeUnit = createModel<TubeUnitModule, TubeUnitWidget>("TubeUnit");
