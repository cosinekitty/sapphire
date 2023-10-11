#include "plugin.hpp"

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


Plugin* pluginInstance;


void init(Plugin* p)
{
    pluginInstance = p;

    p->addModel(modelMoots);
    p->addModel(modelElastika);
    p->addModel(modelTubeUnit);
    p->addModel(modelFrolic);
}
