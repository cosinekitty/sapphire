#include "sapphire_chaos_module.hpp"

// Glee for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    using GleeModule = Sapphire::Chaos::ChaosModule<Aizawa>;
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
