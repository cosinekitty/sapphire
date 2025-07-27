#include "sapphire_chaos_module.hpp"
#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    using ZooModule = Sapphire::Chaos::ChaosModule<ProgOscillator>;
    using ZooWidgetBase = Sapphire::Chaos::ChaosWidget<ZooModule>;
    struct ZooWidget : ZooWidgetBase
    {
        explicit ZooWidget(ZooModule* module)
            : ZooWidgetBase(module, "zoo", "res/zoo.svg")
            {}
    };
}

Model* modelSapphireZoo = createSapphireModel<Sapphire::ZooModule, Sapphire::ZooWidget>(
    "Zoo",
    Sapphire::ChaosModuleRoles
);
