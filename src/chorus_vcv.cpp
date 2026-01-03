#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "chorus_engine.hpp"

namespace Sapphire
{
    namespace Chorus
    {
        enum ParamId
        {
            DEPTH_PARAM,
            DEPTH_ATTEN,
            RATE_PARAM,
            RATE_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_LEFT_INPUT,
            AUDIO_RIGHT_INPUT,
            DEPTH_CV_INPUT,
            RATE_CV_INPUT,
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


        struct ChorusModule : SapphireModule
        {
            Engine engine;

            ChorusModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configControlGroup("Depth", DEPTH_PARAM, DEPTH_ATTEN, DEPTH_CV_INPUT, 0, 1, 0.5);
                configControlGroup("Rate", RATE_PARAM, RATE_ATTEN, RATE_CV_INPUT, -1, +1, 0);
                configInput(AUDIO_LEFT_INPUT, "Audio left");
                configInput(AUDIO_RIGHT_INPUT, "Audio right");
                configOutput(AUDIO_LEFT_OUTPUT, "Audio left");
                configOutput(AUDIO_RIGHT_OUTPUT, "Audio right");
                configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
                configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);
                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void initialize()
            {
                engine.initialize();
            }

            void process(const ProcessArgs& args) override
            {
                float depthKnob = getControlValue(DEPTH_PARAM, DEPTH_ATTEN, DEPTH_CV_INPUT, 0, 1);
                float rateKnob = getControlValue(RATE_PARAM, RATE_ATTEN, RATE_CV_INPUT, -1, +1);
                engine.setChorusDepth(depthKnob);
                engine.setChorusRate(rateKnob);
                float leftInput, rightInput;
                loadStereoInputs(leftInput, rightInput, AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);
                StereoFrame frame = engine.update(args.sampleRate, leftInput, rightInput);
                writeStereoOutputs(frame.sample[0], frame.sample[1], AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT);
            }
        };


        struct ChorusWidget : SapphireWidget
        {
            ChorusModule* chorusModule{};

            explicit ChorusWidget(ChorusModule* module)
                : SapphireWidget("chorus", asset::plugin(pluginInstance, "res/chorus.svg"))
                , chorusModule(module)
            {
                setModule(module);
                addSapphireInput(AUDIO_LEFT_INPUT, "audio_left_input");
                addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");
                addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                addSapphireFlatControlGroup("depth", DEPTH_PARAM, DEPTH_ATTEN, DEPTH_CV_INPUT);
                addSapphireFlatControlGroup("rate", RATE_PARAM, RATE_ATTEN, RATE_CV_INPUT);
            }
        };
    }
}


Model *modelSapphireChorus = createSapphireModel<Sapphire::Chorus::ChorusModule, Sapphire::Chorus::ChorusWidget>(
    "Chorus",
    Sapphire::ExpanderRole::None
);
