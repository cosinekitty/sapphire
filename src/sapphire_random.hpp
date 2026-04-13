#pragma once
#include <random>

namespace Sapphire
{
    class RandomVectorGenerator     // helps generate vectors in an N-dimensional space with unbiased directions
    {
    private:
        uint64_t seed;
        std::mt19937_64 rand;
        std::normal_distribution<float> dist {0.0f, 1.0f};

    public:
        explicit RandomVectorGenerator(uint64_t initSeed)
            : seed(initSeed)
            , rand(initSeed)
            {}

        float next()
        {
            return dist(rand);
        }

        void setSeed(uint64_t newSeed)
        {
            seed = newSeed;
            rand.seed(newSeed);
        }

        uint64_t getSeed() const
        {
            return seed;
        }

        void initialize()
        {
            rand.seed(seed);
        }
    };
}
