#include "sapphire_chaos_module.hpp"

// Lark for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    using LarkModuleBase = Sapphire::Chaos::ChaosModule<DequanLi>;
    struct LarkModule : LarkModuleBase
    {
        explicit LarkModule()
        {
            circuit.cruisingSpeed = 100.0;
        }
    };

    using LarkWidgetBase = Sapphire::Chaos::ChaosWidget<LarkModule>;
    struct LarkWidget : LarkWidgetBase
    {
        explicit LarkWidget(LarkModule* module)
            : LarkWidgetBase(module, "lark", "res/lark.svg")
            {}
    };
}

Model* modelSapphireLark = createSapphireModel<Sapphire::LarkModule, Sapphire::LarkWidget>(
    "Lark",
    Sapphire::ChaosModuleRoles
);
