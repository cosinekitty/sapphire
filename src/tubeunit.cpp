#include "plugin.hpp"
#include "tubeunit_engine.hpp"

// Sapphire Tube Unit for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct TubeUnitModule : Module
{

};


struct TubeUnitWidget : ModuleWidget
{
    TubeUnitModule *tubeUnitModule;

    TubeUnitWidget(TubeUnitModule* module)
    {
        tubeUnitModule = module;
        setModule(module);
    }
};

Model* modelTubeUnit = createModel<TubeUnitModule, TubeUnitWidget>("TubeUnit");
