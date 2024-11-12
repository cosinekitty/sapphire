#include "sapphire_chaos_module.hpp"

// Romp for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    using RompModule = Sapphire::Chaos::ChaosModule<Halvorsen>;
    using RompWidgetBase = Sapphire::Chaos::ChaosWidget<RompModule>;
    struct RompWidget : RompWidgetBase
    {
        explicit RompWidget(RompModule* module)
            : RompWidgetBase(module, "romp", "res/romp.svg")
            {}
    };
}

Model* modelSapphireRomp = createSapphireModel<Sapphire::RompModule, Sapphire::RompWidget>(
    "Romp",
    Sapphire::VectorRole::Sender
);
