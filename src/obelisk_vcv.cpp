#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Obelisk
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            INPUTS_LEN
        };

        enum OutputId
        {
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct ObeliskModule : SapphireModule
        {
            ObeliskModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                initialize();
            }

            void initialize()
            {
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
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

                reloadPanel();
            }
        };
    }
}

Model *modelObelisk = createSapphireModel<Sapphire::Obelisk::ObeliskModule, Sapphire::Obelisk::ObeliskWidget>(
    "Obelisk",
    Sapphire::VectorRole::None
);
