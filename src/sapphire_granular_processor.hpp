/*
    sapphire_granular_processor.hpp  -  Don Cross <cosinekitty@gmail.com>
    Part of the Sapphire project:       https://github.com/cosinekitty/sapphire

    A generalized granular processing framework.
*/
#pragma once
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>


namespace Sapphire
{
    template <typename item_t>
    class BlockHandler
    {
    public:
        virtual void initialize() = 0;
        virtual void onBlock(int length, const item_t* inBlock, item_t* outBlock) = 0;
    };


    class FourierFilter : public BlockHandler<float>
    {
    private:
        const int blockExponent;
        const int blockSize;
        std::vector<float> inSpectrumBuffer;
        std::vector<float> outSpectrumBuffer;

        static int validateBlockExponent(int e)
        {
            const int minExponent = 1;
            const int maxExponent = 30;
            if (e < minExponent || e > maxExponent)
                throw std::invalid_argument(std::string("FFT block-size exponent must be an integer ") + std::to_string(minExponent) + ".." + std::to_string(maxExponent) + ".");
            return e;
        }

        static void transform(
            int direction,      // +1 = forward, -1 = backward
            int blockSize,
            const float *inBlock,
            float *outBlock);

    public:
        FourierFilter(int _blockExponent)
            : blockExponent(validateBlockExponent(_blockExponent))
            , blockSize(1 << _blockExponent)
            , inSpectrumBuffer(1 << _blockExponent)
            , outSpectrumBuffer(1 << _blockExponent)
            {}

        int getBlockSize() const { return blockSize; }
        int getGranuleSize() const { return blockSize / 2; }

        virtual void onSpectrum(int length, const float* inSpectrum, float* outSpectrum) = 0;

        void onBlock(int length, const float* inBlock, float* outBlock) override
        {
            if (length != blockSize)
                throw std::invalid_argument("Incorrect block length sent to FourierFilter.");

            // inSpectrumBuffer := FFT(inBlock)
            transform(+1, blockSize, inBlock, inSpectrumBuffer.data());

            // use callback to mutate spectrum
            onSpectrum(length, inSpectrumBuffer.data(), outSpectrumBuffer.data());

            // outBlock := IFFT(outSpectrumBuffer)
            transform(-1, blockSize, outSpectrumBuffer.data(), outBlock);
        }
    };


    template <typename item_t>
    class GranularProcessor
    {
    private:
        const int granuleSize;
        const int blockSize;
        int index{};
        BlockHandler<item_t>& handler;
        std::vector<item_t> inBlock;
        std::vector<item_t> prevProcBlock;
        std::vector<item_t> currProcBlock;
        std::vector<item_t> fade;

        void calculateFadeFunction()
        {
            // The fade function goes from 1 down to 0 over a granule's worth of samples.
            for (int i = 0; i < granuleSize; ++i)
            {
                double angle = (M_PI * i) / (granuleSize - 1);
                fade[i] = (1 + std::cos(angle)) / 2;
            }
        }

        static int validateGranuleSize(int granuleSize)
        {
            if (granuleSize < 1)
                throw std::invalid_argument("Granule size must be a positive integer.");
            return granuleSize;
        }

    public:
        GranularProcessor(int _granuleSize, BlockHandler<item_t>& blockHandler)
            : granuleSize(validateGranuleSize(_granuleSize))
            , blockSize(2 * _granuleSize)
            , handler(blockHandler)
            , inBlock(2 * _granuleSize)
            , prevProcBlock(2 * _granuleSize)
            , currProcBlock(2 * _granuleSize)
            , fade(_granuleSize)
        {
            calculateFadeFunction();
            initialize();
        }

        const std::vector<item_t>& crossfadeBuffer() const { return fade; }

        void initialize()
        {
            handler.initialize();

            // Assume negative times contain silence stretching back infinitely into the past.
            // Fill the input block with silence.
            for (int i = 0; i < blockSize; ++i)
            {
                inBlock[i] = 0;
                prevProcBlock[i] = 0;
                currProcBlock[i] = 0;
            }

            // Start the input exactly one granule after the initial block of zeroes.
            index = granuleSize;
        }

        item_t process(item_t x, float sampleRateHz)
        {
            if (index == blockSize)
            {
                // The input block is full, so it is time to process it.
                // We are now done with the previous processed block.
                // Swap blocks, because the second half of the current procblock
                // has data we still need to fademix for future samples.
                std::swap(prevProcBlock, currProcBlock);

                // Allow the handler to transform the input block into the current procblock.
                handler.onBlock(blockSize, inBlock.data(), currProcBlock.data());

                // We want to iteratively fill up the second granule in the input block.
                // Move the second input granule into the first.
                for (int i = 0; i < granuleSize; ++i)
                    inBlock.at(i) = inBlock.at(granuleSize + i);

                // Point at the first sample to be filled in the second granule.
                index = granuleSize;
            }

            // Write the input value to the input block.
            inBlock.at(index) = x;

            // Crossfade a sample from the previous procblock and the current procblock.
            item_t f = fade.at(index - granuleSize);
            item_t y1 = prevProcBlock.at(index);
            item_t y2 = currProcBlock.at(index - granuleSize);
            item_t y = f*y1 + (1-f)*y2;

            // Update index for the next iteration.
            ++index;

            return y;
        }
    };


    class FourierProcessor : public GranularProcessor<float>
    {
    public:
        FourierProcessor(FourierFilter& _filter)
            : GranularProcessor<float>(_filter.getGranuleSize(), _filter)
            {}
    };
}
