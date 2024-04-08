#pragma once
#include "sapphire_integrator.hpp"

namespace Sapphire
{
    namespace Slew
    {
        const float MinSpeed = -7;
        const float DefSpeed = 0;
        const float MaxSpeed = +7;

        const float MinViscosity = -1;
        const float MaxViscosity = +1;
        const float DefViscosity = 0;


        class Engine
        {
        private:
            using integrator_t = Sapphire::Integrator::Engine<float>;
            using state_vector_t = integrator_t::state_vector_t;

            const float m = 0.001;      // mass of the particle in [kg]
            const float max_dt = -1;
            integrator_t integrator;

            // Working variables for scratchpad use.
            float mu{};     // damping constant
            float k{};      // spring stiffness
            float r0{};     // target position

        public:
            void initialize()
            {
                integrator.setState(state_vector_t{});
            }

            float operator() (float r, float v) const   // for supporting Integrator::AccelerationFunction
            {
                return -(mu*v + k*(r - r0))/m;
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

                // We are passing parameters to our operator() callback in a sneaky way.
                // This is what I call a hillbilly closure.
                k = m / (tau*tau);
                mu = 2 * (m/tau) * zeta;
                r0 = targetPos;
                for (int i = 0; i < n; ++i)
                    integrator.update(et, *this);

                return integrator.getState();
            }
        };
    }
}
