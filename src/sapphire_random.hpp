#pragma once
#include <random>

namespace Sapphire
{
    class RandomVectorGenerator     // helps generate vectors in an N-dimensional space with unbiased directions
    {
    private:
        std::mt19937 rand;
        std::normal_distribution<float> dist {0.0f, 1.0f};

    public:
        float next()
        {
            return dist(rand);
        }
    };
}
