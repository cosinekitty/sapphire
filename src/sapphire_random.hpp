#pragma once
#include <random>

namespace Sapphire
{
    using SapphireRandom = std::mt19937_64;     // project-wide use of Mersenne Twister pseudo-random generator

    inline float RandomReal(SapphireRandom& rand)       // returns 0 <= x < 1
    {
        return static_cast<float>(rand() >> (64-24)) * 0x1p-24f;
    }

    class RandomVectorGenerator     // helps generate vectors in an N-dimensional space with unbiased directions
    {
    private:
        uint64_t seed;
        SapphireRandom rand;
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
