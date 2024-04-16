#pragma once
#include "sapphire_integrator.hpp"

namespace Sapphire
{
    namespace Slew
    {
        const float MinSpeedKnob = -7;
        const float DefSpeedKnob =  0;
        const float MaxSpeedKnob = +7;

        const float MinViscosityKnob = -1;
        const float DefViscosityKnob =  0;
        const float MaxViscosityKnob = +1;

        class Engine
        {
        private:
            using integrator_t = Sapphire::Integrator::Engine<float>;
            using state_vector_t = integrator_t::state_vector_t;

            const float max_dt = -1;
            integrator_t integrator;

            const float P = 0.1;
            const float Q = 2.0;
            const float A = std::log(Q) - std::log(P);
            const float B = std::log(P);

            inline float Zeta(float viscosityKnob)
            {
                // `viscosityKnob` is a knob value that goes from -1 to +1, with a default value of 0.
                // We want the following fixed points:
                //      Zeta(0) = P
                //      Zeta(1) = Q
                // Modeling this as an exponential function exp(A*x + B), where x = viscosityKnob,
                // we derive:
                //      A = ln(Q) - ln(P)
                //      B = ln(P)
                return std::exp(A*viscosityKnob + B);
            }

        public:
            void initialize()
            {
                integrator.setState(state_vector_t{0, 0});
            }

            state_vector_t process(float dt, float targetPos, float viscosityKnob)
            {
                // `targetPos` is the position the particle is being pulled toward by the spring.
                // Determine how much oversampling we need for reliable stability and accuracy.
                const int n = (max_dt <= 0.0) ? 1 : static_cast<int>(std::ceil(dt / max_dt));
                const double et = dt / n;

                float zeta = Zeta(viscosityKnob);
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
