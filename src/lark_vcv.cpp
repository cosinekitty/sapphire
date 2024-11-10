#include "sapphire_chaos_module.hpp"

// Lark for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    using LarkModule = Sapphire::Chaos::ChaosModule<DequanLi>;
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
    Sapphire::VectorRole::Sender
);
