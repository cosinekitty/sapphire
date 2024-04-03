#include "plugin.hpp"

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


Plugin* pluginInstance;


void init(Plugin* p)
{
    pluginInstance = p;

    p->addModel(modelBash);
    p->addModel(modelElastika);
    p->addModel(modelFrolic);
    p->addModel(modelGlee);
    p->addModel(modelHiss);
    p->addModel(modelMoots);
    p->addModel(modelNucleus);
    p->addModel(modelPolynucleus);
    p->addModel(modelSpatula);
    p->addModel(modelTin);
    p->addModel(modelTout);
    p->addModel(modelTricorder);
    p->addModel(modelTubeUnit);
}
