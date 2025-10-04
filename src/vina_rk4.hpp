//*** GENERATED CODE - !!! DO NOT EDIT !!! ***
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nRows = 2;
        constexpr unsigned nMobileColumns = 13;
        constexpr unsigned nColumns = nMobileColumns + 2;

        struct VinaStereoFrame
        {
            float sample[2];

            explicit VinaStereoFrame(float left, float right)
                : sample{left, right}
                {}
        };

        class VinaEngine
        {
        public:
            explicit VinaEngine()
            {
            }

            void initialize()
            {
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
        };
    }
}
