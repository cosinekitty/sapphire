#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "galaxy_engine.hpp"

// Sapphire Galaxy for VCV Rack by Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire
//
// An adaptation of Airwindows Galactic by Chris Johnson:
// https://github.com/airwindows/airwindows

namespace Sapphire
{
    namespace Galaxy
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_LEFT_INPUT,
            AUDIO_RIGHT_INPUT,
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

        struct GalaxyModule : SapphireModule
        {
            Engine engine;

            GalaxyModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(AUDIO_LEFT_INPUT, "Audio left");
                configInput(AUDIO_RIGHT_INPUT, "Audio right");

                configInput(AUDIO_LEFT_OUTPUT, "Audio left");
                configInput(AUDIO_RIGHT_OUTPUT, "Audio right");

                initialize();
            }

            void initialize()
            {
                engine.initialize();

                // FIXFIXFIX - hack!
                ReverbParameters& parms = engine.parameters();
                parms.A = 0.5;
                parms.B = 0.5;
                parms.C = 0.5;
                parms.D = 0.5;
                parms.E = 0.5;
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                const float inLeft  = inputs[AUDIO_LEFT_INPUT ].getVoltageSum();
                const float inRight = inputs[AUDIO_RIGHT_INPUT].getVoltageSum();
                float outLeft, outRight;
                engine.process(args.sampleRate, inLeft, inRight, outLeft, outRight);
                outputs[AUDIO_LEFT_OUTPUT ].setVoltage(outLeft);
                outputs[AUDIO_RIGHT_OUTPUT].setVoltage(outRight);
            }
        };

        struct GalaxyWidget : SapphireReloadableModuleWidget
        {
            GalaxyModule* galaxyModule{};

            explicit GalaxyWidget(GalaxyModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/galaxy.svg"))
                , galaxyModule(module)
            {
                setModule(module);

                addSapphireInput(AUDIO_LEFT_INPUT, "audio_left_input");
                addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");

                addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");

                reloadPanel();
            }
        };
    }
}

Model *modelGalaxy = createSapphireModel<Sapphire::Galaxy::GalaxyModule, Sapphire::Galaxy::GalaxyWidget>(
    "Galaxy",
    Sapphire::VectorRole::None
);
