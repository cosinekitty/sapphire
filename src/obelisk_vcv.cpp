#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "obelisk_engine.hpp"

// Sapphire Obelisk for VCV Rack by Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Obelisk
    {
        const int NumParticles = 64;
        using engine_t = Engine<NumParticles>;

        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            LEFT_AUDIO_INPUT,
            RIGHT_AUDIO_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            LEFT_AUDIO_OUTPUT,
            RIGHT_AUDIO_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct ObeliskModule : SapphireModule
        {
            engine_t engine;
            float drive = 1.0e-3;
            float level = 1/drive;
            int inputBallIndex = 3;
            int outputBallIndex = NumParticles - (inputBallIndex+1);

            ObeliskModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(LEFT_AUDIO_INPUT, "Left audio");
                configInput(RIGHT_AUDIO_INPUT, "Right audio");
                configOutput(LEFT_AUDIO_OUTPUT, "Left audio");
                configOutput(RIGHT_AUDIO_OUTPUT, "Right audio");

                initialize();
            }

            void initialize()
            {
                engine.initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                PhysicsVector& invel = engine.velocity(inputBallIndex);
                invel[1] += drive * inputs[LEFT_AUDIO_INPUT ].getVoltageSum();
                invel[2] += drive * inputs[RIGHT_AUDIO_INPUT].getVoltageSum();

                engine.process(args.sampleRate);

                const PhysicsVector& outvel = engine.velocity(outputBallIndex);
                outputs[LEFT_AUDIO_OUTPUT ].setVoltage(level * outvel[1]);
                outputs[RIGHT_AUDIO_OUTPUT].setVoltage(level * outvel[2]);
            }
        };


        struct ObeliskWidget : SapphireReloadableModuleWidget
        {
            ObeliskModule* obeliskModule{};

            explicit ObeliskWidget(ObeliskModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/obelisk.svg"))
                , obeliskModule(module)
            {
                setModule(module);
                addSapphireInput(LEFT_AUDIO_INPUT, "left_audio_input");
                addSapphireInput(RIGHT_AUDIO_INPUT, "right_audio_input");
                addSapphireOutput(LEFT_AUDIO_OUTPUT, "left_audio_output");
                addSapphireOutput(RIGHT_AUDIO_OUTPUT, "right_audio_output");
                reloadPanel();
            }
        };
    }
}

Model *modelObelisk = createSapphireModel<Sapphire::Obelisk::ObeliskModule, Sapphire::Obelisk::ObeliskWidget>(
    "Obelisk",
    Sapphire::VectorRole::None
);
