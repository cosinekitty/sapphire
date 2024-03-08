#include "plugin.hpp"

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


namespace Sapphire
{
    // Put any global variables here that are needed across the entire Sapphire plugin.
    // Don't put them in our src/plugin.cpp because that file is excluded from Cardinal builds.
    // Cardinal's Makefile has the following command to filter out our plugin.cpp:
    //
    //     PLUGIN_FILES += $(filter-out Sapphire/src/plugin.cpp,$(wildcard Sapphire/src/*.cpp))
    //
    ModelInfo *ModelInfo::front;
}
