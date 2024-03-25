/*
    sapphire_granular_processor.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#include "sapphire_granular_processor.hpp"
#include "pffft.h"

namespace Sapphire
{
    void FourierFilter::allocateBuffers()
    {
        inSpectrumBuffer = allocAlignedBuffer(blockSize);
        outSpectrumBuffer = allocAlignedBuffer(blockSize);
        workBlock = allocAlignedBuffer(blockSize);
        fft = pffft_new_setup(blockSize, PFFFT_REAL);
        if (fft == nullptr)
            throw std::runtime_error("pffft_new_setup() returned null.");
    }

    void FourierFilter::freeBuffers()
    {
        if (fft != nullptr)
        {
            pffft_destroy_setup(static_cast<PFFFT_Setup*>(fft));
            fft = nullptr;
        }
        freeAlignedBuffer(workBlock);
        freeAlignedBuffer(outSpectrumBuffer);
        freeAlignedBuffer(inSpectrumBuffer);
        blockExponent = -1;
        blockSize = -1;
    }

    float *FourierFilter::allocAlignedBuffer(int blockSize)
    {
        void *ptr = pffft_aligned_malloc(blockSize * sizeof(float));
        if (ptr == nullptr)
            throw std::runtime_error("pffft_aligned_malloc() returned null.");
        float *buffer = static_cast<float*>(ptr);
        for (int i = 0; i < blockSize; ++i)
            buffer[i] = 0;
        return buffer;
    }

    void FourierFilter::freeAlignedBuffer(float*& buffer)
    {
        if (buffer != nullptr)
        {
            pffft_aligned_free(buffer);
            buffer = nullptr;
        }
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
        // FIXFIXFIX: could be faster using SIMD.
        for (int i = 0; i < blockSize; ++i)
            outBlock[i] /= blockSize;
    }
}
