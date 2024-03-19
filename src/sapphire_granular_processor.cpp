/*
    sapphire_granular_processor.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#include "sapphire_granular_processor.hpp"

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

        pffft_direction_t pdir = (direction < 0) ? PFFFT_BACKWARD : PFFFT_FORWARD;
        pffft_transform_ordered(fft, inBlock, outBlock, workBlock.data(), pdir);
    }

    void FourierFilter::initialize()
    {
        if (fft == nullptr)
        {
            fft = pffft_new_setup(blockSize, PFFFT_REAL);
            if (fft == nullptr)
                throw std::runtime_error("pffft_new_setup() returned null.");
        }
    }
}
