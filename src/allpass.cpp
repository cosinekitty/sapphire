#include <cmath>
#include "plugin.hpp"
#include "sapphire_engine.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Allpass
    {
        enum ParamId
        {
            DELAY_KNOB_PARAM,
            DELAY_ATTEN_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            DELAY_CV_INPUT,
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
                configParam(DELAY_KNOB_PARAM, 0, 1, 0.5, "Delay");
                configAttenuverter(DELAY_ATTEN_PARAM, "Delay");
                configInput(DELAY_CV_INPUT, "Delay");
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

            static inline float delaySecondsFromKnob(float knob)
            {
                constexpr float minDelaySeconds = 0.001;
                constexpr float maxDelaySeconds = 1.0;
                return minDelaySeconds * std::pow(maxDelaySeconds/minDelaySeconds, knob);
            }

            void process(const ProcessArgs& args) override
            {
                const int nc = numOutputChannels(INPUTS_LEN);
                outputs[AUDIO_OUTPUT].setChannels(nc);
                float gain = 0.995;
                float inAudioSample = 0;
                float cvDelay = 0;
                for (int c = 0; c < nc; ++c)
                {
                    nextChannelInputVoltage(inAudioSample, AUDIO_INPUT, c);
                    nextChannelInputVoltage(cvDelay, DELAY_CV_INPUT, c);
                    float delayKnob = cvGetControlValue(DELAY_KNOB_PARAM, DELAY_ATTEN_PARAM, cvDelay, 0, 1);
                    float delayFractionalSamples = args.sampleRate * delaySecondsFromKnob(delayKnob);
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
                addSapphireControlGroup("delay", DELAY_KNOB_PARAM, DELAY_ATTEN_PARAM, DELAY_CV_INPUT);
                reloadPanel();
            }
        };
    }
}


Model* modelAllpass = createSapphireModel<Sapphire::Allpass::AllpassModule, Sapphire::Allpass::AllpassWidget>(
    "Allpass",
    Sapphire::VectorRole::None
);
