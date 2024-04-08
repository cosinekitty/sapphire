#pragma once
#include "sapphire_integrator.hpp"

namespace Sapphire
{
    namespace Slew
    {
        const float MinSpeed = -7;
        const float DefSpeed =  0;
        const float MaxSpeed = +7;

        const float MinViscosity = -1;
        const float DefViscosity =  0;
        const float MaxViscosity = +1;

        class Engine
        {
        private:
            using integrator_t = Sapphire::Integrator::Engine<float>;
            using state_vector_t = integrator_t::state_vector_t;

            const float max_dt = -1;
            integrator_t integrator;

        public:
            void initialize()
            {
                integrator.setState(state_vector_t{0, 0});
            }

            state_vector_t process(float dt, float targetPos, float viscosity)
            {
                // `targetPos` is the position the particle is being pulled toward by the spring.

                // `viscosity` is a knob value that goes from -1 to +1, with a default value of 0.
                // So it can mean whatever we want.
                // In this case, we want it to mean that the default (0) indicates critical damping.
                // -1 should be nearly frictionless and oscillating for a long time after an impulse.
                // +1 should be deeply damped, where it takes forever to reach a new step level.

                // Determine how much oversampling we need for reliable stability and accuracy.
                const int n = (max_dt <= 0.0) ? 1 : static_cast<int>(std::ceil(dt / max_dt));
                const double et = dt / n;

                const float factor = 1;     // FIXFIXFIX: what should this be, to convert knob to zeta?
                float zeta = std::max(0.0f, factor*viscosity + 1);
                const float tau = 0.003;     // time constant in seconds
                const float k_over_m = 1 / (tau*tau);
                const float mu_over_m  = (2*zeta) / tau;
                const float r0  = targetPos;

                auto accel = [mu_over_m, k_over_m, r0] (float r, float v) -> float
                {
                    return -(mu_over_m*v + k_over_m*(r - r0));
                };

                for (int i = 0; i < n; ++i)
                    integrator.update(et, accel);

                return integrator.getState();
            }
        };
    }
}
