#include "plugin.hpp"

struct Elastika : Module
{

};


struct ElastikaWidget : ModuleWidget
{
    ElastikaWidget(Elastika* module)
    {
        setModule(module);
    }
};


Model* modelElastika = createModel<Elastika, ElastikaWidget>("Elastika");

