#include "plugin.hpp"

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


Plugin* pluginInstance;


void init(Plugin* p)
{
    pluginInstance = p;

    p->addModel(modelSapphireChaops);
    p->addModel(modelSapphireEcho);
    p->addModel(modelSapphireEchoOut);
    p->addModel(modelSapphireEchoTap);
    p->addModel(modelSapphireElastika);
    p->addModel(modelSapphireEnv);
    p->addModel(modelSapphireFrolic);
    p->addModel(modelSapphireGalaxy);
    p->addModel(modelSapphireGlee);
    p->addModel(modelSapphireGravy);
    p->addModel(modelSapphireHiss);
    p->addModel(modelSapphireMoots);
    p->addModel(modelSapphireLark);
    p->addModel(modelSapphireNucleus);
    p->addModel(modelSapphirePivot);
    p->addModel(modelSapphirePolynucleus);
    p->addModel(modelSapphirePop);
    p->addModel(modelSapphireRotini);
    p->addModel(modelSapphireSam);
    p->addModel(modelSapphireSauce);
    p->addModel(modelSapphireTin);
    p->addModel(modelSapphireTout);
    p->addModel(modelSapphireTricorder);
    p->addModel(modelSapphireTubeUnit);
}
