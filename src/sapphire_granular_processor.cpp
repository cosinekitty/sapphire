/*
    sapphire_granular_processor.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#include "sapphire_granular_processor.hpp"
#include "pffft.h"

namespace Sapphire
{
    static float *AllocAlignedBuffer(int blockSize)
    {
        void *ptr = pffft_aligned_malloc(blockSize * sizeof(float));
        if (ptr == nullptr)
            throw std::runtime_error("pffft_aligned_malloc() returned null.");
        float *buffer = static_cast<float*>(ptr);
        for (int i = 0; i < blockSize; ++i)
            buffer[i] = 0;
        return buffer;
    }

    FourierFilter::FourierFilter(int _blockExponent)
        : blockExponent(validateBlockExponent(_blockExponent))
        , blockSize(1 << _blockExponent)
    {
        inSpectrumBuffer = AllocAlignedBuffer(blockSize);
        outSpectrumBuffer = AllocAlignedBuffer(blockSize);
        workBlock = AllocAlignedBuffer(blockSize);
        fft = pffft_new_setup(blockSize, PFFFT_REAL);
        if (fft == nullptr)
            throw std::runtime_error("pffft_new_setup() returned null.");
    }

    FourierFilter::~FourierFilter()
    {
        pffft_destroy_setup(static_cast<PFFFT_Setup*>(fft));
        pffft_aligned_free(workBlock);
        pffft_aligned_free(outSpectrumBuffer);
        pffft_aligned_free(inSpectrumBuffer);
    }

    void FourierFilter::forwardTransform(const float *inBlock, float *outBlock)
    {
        pffft_transform_ordered(static_cast<PFFFT_Setup*>(fft), inBlock, outBlock, workBlock, PFFFT_FORWARD);
    }

    void FourierFilter::reverseTransform(const float *inBlock, float *outBlock)
    {
        pffft_transform_ordered(static_cast<PFFFT_Setup*>(fft), inBlock, outBlock, workBlock, PFFFT_BACKWARD);

        // pffft.h says: "Transforms are not scaled: PFFFT_BACKWARD(PFFFT_FORWARD(x)) = N*x."
        // So we normalize here by dividing by the block size.
        for (int i = 0; i < blockSize; ++i)
            outBlock[i] /= blockSize;
    }
}
