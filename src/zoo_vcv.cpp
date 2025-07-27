#include "sapphire_chaos_module.hpp"
#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    using ZooModuleBase = Sapphire::Chaos::ChaosModule<ProgOscillator>;
    struct ZooModule : ZooModuleBase
    {
        explicit ZooModule()
            : ZooModuleBase()
        {
            circuit.xCompile("-y-z");
            circuit.yCompile("x+a*y");
            circuit.zCompile("b+z*(x-c)");
        }
    };


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
