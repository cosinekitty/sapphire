//*** GENERATED CODE - !!! DO NOT EDIT !!! ***
#pragma once
#include "sapphire_simd.hpp"
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nRows = 2;
        constexpr unsigned nMobileColumns = 13;
        constexpr unsigned nColumns = 15;
        constexpr unsigned nParticles = nColumns * nRows;
        constexpr unsigned nMobileParticles = nMobileColumns * nRows;

        constexpr float horSpace = 0.01;    // horizontal spacing in meters
        constexpr float verSpace = 0.01;    // vertical spacing in meters

        inline unsigned particleIndex(unsigned c, unsigned r)
        {
            return r + c*nRows;
        }

        struct VinaStereoFrame
        {
            float sample[2];

            explicit VinaStereoFrame(float left, float right)
                : sample{left, right}
                {}
        };

        struct VinaParticle
        {
            PhysicsVector pos;
            PhysicsVector vel;
        };

        class VinaEngine
        {
        public:
            explicit VinaEngine()
            {
            }

            void initialize()
            {
                for (unsigned r = 0; r < nRows; ++r)
                {
                    for (unsigned c = 0; c < nColumns; ++c)
                    {
                        unsigned i = particleIndex(c, r);
                        particle[i].pos = PhysicsVector{horSpace*c, verSpace*r, 0, 0};
                        particle[i].vel = PhysicsVector{0, 0, 0, 0};
                    }
                }

            }

            void pluck()
            {
            }

            VinaStereoFrame update(float sampleRateHz)
            {
                float left = 0;
                float right = 0;
                return VinaStereoFrame{left, right};
            }

        private:
            VinaParticle particle[nParticles];
        };
    }
}
