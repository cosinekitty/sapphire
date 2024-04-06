#pragma once

namespace Sapphire
{
    namespace Slew
    {
        const float MinSpeed = -7;
        const float DefaultSpeed = 0;
        const float MaxSpeed = +7;

        const float MinViscosity = 0;
        const float MaxViscosity = 1;
        const float DefaultViscosity = 0.5;


        class Engine
        {
        private:

        public:
            void initialize()
            {
            }

            float process(float samplingRateHz, float target, float speed, float viscosity)
            {
                return target;        // FIXFIXFIX: replace with slewing wizardry
            }
        };
    }
}
