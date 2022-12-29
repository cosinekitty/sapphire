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
        using namespace Sapphire;

        int tubeVoctChannels = inputs[TUBE_VOCT_INPUT].getChannels();
        int formantVoctChannels = inputs[FORMANT_VOCT_INPUT].getChannels();
        int voiceGateChannels = inputs[VOICE_GATE_INPUT].getChannels();

        // The current number of channels is determined
        // by the most polyphonic of the following input cables:
        // - Tube V/OCT
        // - Formant V/OCT
        // - Voice Gate
        // But still present one channel of output if there are no inputs.
        int outputChannels = std::max({1, tubeVoctChannels, formantVoctChannels, voiceGateChannels});

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
        // For example, if Tube V/OCT has values [3.5, 2.5, 4.1],
        // Formant V/OCT has values [2.1, -1.2],
        // and Voice Gate has the single value [5.0],
        // then the effective number of channels is 3 and Format V/OCT
        // is interpreted as [2.1, -1.2, -1.2], and Voice Gate is interpreted
        // as [5.0, 5.0, 5.0].
        float tubeFreqHz = TubeUnitDefaultRootFrequencyHz;
        float formantFreqHz = TubeUnitDefaultFormantFrequencyHz;
        bool active = true;

        for (int c = 0; c < outputChannels; ++c)
        {
            if (c < tubeVoctChannels)
                tubeFreqHz = FrequencyFromVoct(inputs[TUBE_VOCT_INPUT].getVoltage(c), TubeUnitDefaultRootFrequencyHz);

            if (c < formantVoctChannels)
                formantFreqHz = FrequencyFromVoct(inputs[FORMANT_VOCT_INPUT].getVoltage(c), TubeUnitDefaultFormantFrequencyHz);

            if (c < voiceGateChannels)
            {
                // Schmitt Trigger
                float gate = inputs[VOICE_GATE_INPUT].getVoltage(c);
                if (gate <= 0.1f)
                    active = false;
                else if (gate >= 1.0f)
                    active = true;
                else
                    active = engine[c].getActive();
            }

            engine[c].setActive(active);
            engine[c].setRootFrequency(tubeFreqHz);
            engine[c].setFormantFrequency(formantFreqHz);

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
