/*
    sapphire_granular_processor.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#include "sapphire_granular_processor.hpp"
#include "pffft.h"

namespace Sapphire
{
    void FourierFilter::transform(
        int direction,      // +1 = forward, -1 = backward
        int blockSize,
        const float *inBlock,
        float *outBlock)
    {
        // Short-term hack until I get pffft working.
        for (int i = 0; i < blockSize; ++i)
            outBlock[i] = inBlock[i];
    }
}
