#include "plugin.hpp"

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


Plugin* pluginInstance;


void init(Plugin* p)
{
    pluginInstance = p;

    p->addModel(modelElastika);
    p->addModel(modelFrolic);
    p->addModel(modelGalaxy);
    p->addModel(modelGlee);
    p->addModel(modelHiss);
    p->addModel(modelMoots);
    p->addModel(modelNucleus);
    p->addModel(modelObelisk);
    p->addModel(modelPolynucleus);
    p->addModel(modelRotini);
    p->addModel(modelTin);
    p->addModel(modelTout);
    p->addModel(modelTricorder);
    p->addModel(modelTubeUnit);
}
