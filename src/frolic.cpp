#include "sapphire_chaos_module.hpp"

// Frolic for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    using FrolicModule = Sapphire::Chaos::ChaosModule<Rucklidge>;
    using FrolicWidgetBase = Sapphire::Chaos::ChaosWidget<FrolicModule>;
    struct FrolicWidget : FrolicWidgetBase
    {
        explicit FrolicWidget(FrolicModule* module)
            : FrolicWidgetBase(module, "res/frolic.svg")
            {}
    };
}

Model* modelFrolic = createSapphireModel<Sapphire::FrolicModule, Sapphire::FrolicWidget>(
    "Frolic",
    Sapphire::VectorRole::Sender
);
