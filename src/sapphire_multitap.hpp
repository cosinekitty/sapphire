#pragma once
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct Message
        {
            int nchannels = 0;
            float audio[PORT_MAX_CHANNELS]{};
        };
    }
}
