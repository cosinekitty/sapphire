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
            : GleeWidgetBase(module, "res/glee.svg")
            {}
    };
}

Model* modelGlee = createSapphireModel<Sapphire::GleeModule, Sapphire::GleeWidget>(
    "Glee",
    Sapphire::VectorRole::Sender
);
