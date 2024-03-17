/*
    sapphire_granular_processor.hpp  -  Don Cross <cosinekitty@gmail.com>
    Part of the Sapphire project:       https://github.com/cosinekitty/sapphire

    A generalized granular processing framework.
*/

#pragma once
#include <stdexcept>
#include <cmath>

namespace Sapphire
{
    template <typename item_t>
    class BlockHandler
    {
    public:
        virtual void initialize() = 0;
        virtual void onBlock(int length, const item_t* inBlock, item_t* outBlock) = 0;
    };


    template <typename item_t>
    class GranularProcessor
    {
    private:
        const int granuleSize;
        const int blockSize;
        int inputIndex{};
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
                inBlock[i] = 0;

            // Don't assume that processed silence is also silence.
            // Ask the block handler to process silence and retain the result.
            handler.onBlock(blockSize, inBlock.data(), currProcBlock.data());

            // We start out with a block full of zeroes, so start in the block-full state.
            inputIndex = blockSize;
        }

        item_t process(item_t x, float sampleRateHz)
        {
            if (inputIndex == blockSize)
            {
                // The input block is full, so it is time to process it.
                // We are now done with the previous processed block ("procblock").
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
                inputIndex = granuleSize;
            }

            // Write the input value to the input block.
            inBlock.at(inputIndex) = x;

            // Crossfade a sample from the previous procblock and the current procblock.
            item_t f = fade.at(inputIndex - granuleSize);
            item_t y1 = prevProcBlock.at(inputIndex);
            item_t y2 = currProcBlock.at(inputIndex - granuleSize);
            item_t y = f*y1 + (1-f)*y2;

            // Update index for the next iteration.
            ++inputIndex;

            return y;
        }
    };
}
