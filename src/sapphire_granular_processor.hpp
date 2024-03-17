#pragma once
#include <stdexcept>
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
    class BlockProcessor
    {
    private:
        const int granuleSize;
        const int blockSize;
        BlockHandler<item_t>& handler;
        std::vector<item_t> inBlock;
        int inputIndex{};
        std::vector<item_t> prevProcBlock;
        std::vector<item_t> currProcBlock;

        static int validateGranuleSize(int granuleSize)
        {
            if (granuleSize < 1)
                throw std::invalid_argument("Granule size must be a positive integer.");
            return granuleSize;
        }

    public:
        BlockProcessor(int _granuleSize, BlockHandler<item_t>& blockHandler)
            : granuleSize(validateGranuleSize(_granuleSize))
            , blockSize(2 * _granuleSize)
            , handler(blockHandler)
            , inBlock(2 * _granuleSize)
            , prevProcBlock(2 * _granuleSize)
            , currProcBlock(2 * _granuleSize)
        {
            initialize();
        }

        void initialize()
        {
            handler.initialize();

            // Assume negative times contain silence stretching back infinitely into the past.
            // Fill the input block with silence.
            for (int i = 0; i < blockSize; ++i)
                inBlock[i] = 0;

            // Don't assume that processed silence is also silence.
            // Ask the block handler to process silence and retain the result.
            handler.onBlock(blockSize, inBlock.data(), prevProcBlock.data());

            // Get ready to put new input into inBlock.
            // One block = two granules.
            // We start at the halfway point (beginning of the second granule in the block).
            inputIndex = granuleSize;
        }

        item_t process(item_t x, float sampleRateHz)
        {
            return 0;       // FIXFIXFIX
        }
    };
}
