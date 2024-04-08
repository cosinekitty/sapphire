// sapphire_integrator.hpp  -  Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire synth plugin project:
// https://github.com/cosinekitty/sapphire
#pragma once
#include <functional>

namespace Sapphire
{
    namespace Integrator
    {
        // `value_t` may be scalar or vector, so long as it supplies all the needed operators.

        template <typename value_t>
        struct StateVector
        {
            value_t r{};      // position, or any function of time r(t)
            value_t v{};      // velocity, or v(t) = r'(t) = dr/dt
        };

        // An acceleration function is a very general concept.
        // It could convert the 1D position and velocity of a ball on a spring
        // into the 1D acceleration of the ball (damped harmonic motion).
        // It could operate on 3D position and velocity of a single particle.
        // Or value_t could represent the position of N particles in a grid!
        // An entire mesh could be computed this way.
        template <typename value_t>
        using AccelerationFunction = std::function<value_t(value_t /*position*/, value_t /*velocity*/)>;


        template <typename value_t>
        class Engine
        {
        public:
            using state_vector_t = StateVector<value_t>;
            using accel_function_t = AccelerationFunction<value_t>;

        private:
            state_vector_t state{};

            state_vector_t derivative(value_t r, value_t v, accel_function_t accel) const
            {
                state_vector_t deriv;
                deriv.r = v;
                deriv.v = accel(r, v);
                return deriv;
            }

        public:
            void setState(const state_vector_t& newState)
            {
                state = newState;
            }

            state_vector_t getState() const
            {
                return state;
            }

            state_vector_t update(float dt, accel_function_t accel)
            {
                // This Runge-Kutta method is adapted from a Microsoft Copilot conversation:
                // https://copilot.microsoft.com/sl/gb6qoDZ1ELQ
                state_vector_t k1 = derivative(state.r, state.v, accel);
                state_vector_t k2 = derivative(state.r + (dt/2)*k1.r, state.v + (dt/2)*k1.v, accel);
                state_vector_t k3 = derivative(state.r + (dt/2)*k2.r, state.v + (dt/2)*k2.v, accel);
                state_vector_t k4 = derivative(state.r + (dt  )*k3.r, state.v + (dt  )*k3.v, accel);
                state.r += (dt/6)*(k1.r + 2*k2.r + 2*k3.r + k4.r);
                state.v += (dt/6)*(k1.v + 2*k2.v + 2*k3.v + k4.v);
                return state;
            }
        };
    }
}
