#include "plugin.hpp"
#include "sapphire_spatula_engine.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Spatula
    {
        const int BlockExponent = 14;       // 2^BlockExponent samples per block

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

        struct SpatulaModule : SapphireModule
        {
            FrameProcessor engine{BlockExponent};

            SpatulaModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                initialize();
            }

            void initialize()
            {
                engine.initialize();

                // FIXFIXFIX: wire these settings up to control groups!
                //engine.setBandDispersion(2, 30);
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                auto &input = inputs[AUDIO_INPUT];
                auto &output = outputs[AUDIO_OUTPUT];

                FrameProcessor::frame_t inFrame;
                inFrame.length = input.getChannels();
                for (int c = 0; c < inFrame.length; ++c)
                    inFrame.data[c] = input.getVoltage(c);

                FrameProcessor::frame_t outFrame;
                engine.process(args.sampleRate, inFrame, outFrame);

                output.setChannels(inFrame.length);
                for (int c = 0; c < outFrame.length; ++c)
                    output.setVoltage(outFrame.data[c], c);
            }
        };


        struct SpatulaWidget : SapphireReloadableModuleWidget
        {
            SpatulaModule *spatulaModule{};

            explicit SpatulaWidget(SpatulaModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/spatula.svg"))
                , spatulaModule(module)
            {
                setModule(module);
                addSapphireInput(AUDIO_INPUT, "audio_input");
                addSapphireOutput(AUDIO_OUTPUT, "audio_output");
                reloadPanel();
            }
        };
    }
}


Model* modelSpatula = createSapphireModel<Sapphire::Spatula::SpatulaModule, Sapphire::Spatula::SpatulaWidget>(
    "Spatula",
    Sapphire::VectorRole::None
);
