#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

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

        struct GalaxyModule : SapphireModule
        {
            GalaxyModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
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

                // add controls here...

                reloadPanel();
            }
        };
    }
}

Model *modelGalaxy = createSapphireModel<Sapphire::Galaxy::GalaxyModule, Sapphire::Galaxy::GalaxyWidget>(
    "Galaxy",
    Sapphire::VectorRole::None
);
