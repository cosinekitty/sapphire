#include "plugin.hpp"

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


Plugin* pluginInstance;


void init(Plugin* p)
{
    pluginInstance = p;

    // Add modules here
    p->addModel(modelMoots);
    p->addModel(modelElastika);

    // Any other plugin initialization may go here.
    // As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
