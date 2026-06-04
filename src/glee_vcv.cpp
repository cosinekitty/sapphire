#include "sapphire_chaos_module.hpp"

// Glee for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    using GleeModuleBase = Sapphire::Chaos::ChaosModule<Aizawa>;
    struct GleeModule : GleeModuleBase
    {
        explicit GleeModule()
        {
            circuit.cruisingSpeed = 1.8;        // must keep in sync with 'presets/Zoo/Aizawa (Glee).vcvm'
        }
    };

    using GleeWidgetBase = Sapphire::Chaos::ChaosWidget<GleeModule>;
    struct GleeWidget : GleeWidgetBase
    {
        explicit GleeWidget(GleeModule* module)
            : GleeWidgetBase(module, "glee", "res/glee.svg")
            {}
    };
}

Model* modelSapphireGlee = createSapphireModel<Sapphire::GleeModule, Sapphire::GleeWidget>(
    "Glee",
    Sapphire::ChaosModuleRoles
);
