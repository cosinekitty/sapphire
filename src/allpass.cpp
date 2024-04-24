#include "plugin.hpp"
#include "sapphire_engine.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Allpass
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct AllpassModule : SapphireModule
        {
            using filter_t = AllpassFilter<float, float>;
            filter_t filter[PORT_MAX_CHANNELS];

            AllpassModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(AUDIO_INPUT, "Audio");
                configOutput(AUDIO_OUTPUT, "Audio");
                configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
                initialize();
            }

            void initialize()
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    filter[c].clear();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                const int nc = numOutputChannels(INPUTS_LEN);
                outputs[AUDIO_OUTPUT].setChannels(nc);
                float gain = 0.985;
                float delaySeconds = 0.005712173650078962;
                float delayFractionalSamples = args.sampleRate * delaySeconds;
                float inAudioSample = 0;
                for (int c = 0; c < nc; ++c)
                {
                    nextChannelInputVoltage(inAudioSample, AUDIO_INPUT, c);
                    float y = filter[c].process(inAudioSample, gain, delayFractionalSamples);
                    outputs[AUDIO_OUTPUT].setVoltage(y, c);
                }
            }
        };


        struct AllpassWidget : SapphireReloadableModuleWidget
        {
            explicit AllpassWidget(AllpassModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/allpass.svg"))
            {
                setModule(module);
                addSapphireInput(AUDIO_INPUT, "audio_input");
                addSapphireOutput(AUDIO_OUTPUT, "audio_output");
                reloadPanel();
            }
        };
    }
}


Model* modelAllpass = createSapphireModel<Sapphire::Allpass::AllpassModule, Sapphire::Allpass::AllpassWidget>(
    "Allpass",
    Sapphire::VectorRole::None
);
