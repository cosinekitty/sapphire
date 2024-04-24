#include "plugin.hpp"
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
            AllpassModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(AUDIO_INPUT, "Audio");
                configOutput(AUDIO_OUTPUT, "Audio");
                initialize();
            }

            void initialize()
            {
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
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
