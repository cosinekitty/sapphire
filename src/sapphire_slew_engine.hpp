#pragma once

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

        struct ParticleState
        {
            float pos;
            float vel;

            ParticleState()
                : pos(0)
                , vel(0)
                {}

            explicit ParticleState(float _pos, float _vel)
                : pos(_pos)
                , vel(_vel)
                {}
        };


        inline ParticleState derivative(float pos, float vel, float mass, float damp, float spring)
        {
            return ParticleState {
                vel,
                (damp/mass)*vel - (spring/mass)*pos
            };
        }


        class Engine
        {
        private:
            const float m = 0.001;      // mass of the particle in [kg]
            const float max_dt = -1;
            ParticleState particle;

            void step(float dt, float targetPos, float mu, float k)
            {
                // This Runge-Kutta simulation is adapted from a Microsoft Copilot conversation:
                // https://copilot.microsoft.com/sl/gb6qoDZ1ELQ
                float pos = particle.pos - targetPos;
                float vel = particle.vel;
                ParticleState k1 = derivative(pos, vel, m, mu, k);
                ParticleState k2 = derivative(pos + dt/2*k1.pos, vel + dt/2*k1.vel, m, mu, k);
                ParticleState k3 = derivative(pos + dt/2*k2.pos, vel + dt/2*k2.vel, m, mu, k);
                ParticleState k4 = derivative(pos + dt*k3.pos,   vel + dt*k3.vel,   m, mu, k);
                float dx = (dt/6)*(k1.pos + 2*k2.pos + 2*k3.pos + k4.pos);
                float dv = (dt/6)*(k1.vel + 2*k2.vel + 2*k3.vel + k4.vel);
                particle.pos += dx;
                particle.vel += dv;
            }

        public:
            void initialize()
            {
                particle = ParticleState{};
            }

            ParticleState process(float dt, float targetPos, float viscosity)
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

                // double zeta = mu / (2*sqrt(m*k));  // damping ratio
                // mu = 2*sqrt(m*k)*zeta
                const float factor = 1;     // FIXFIXFIX: what should this be, to convert knob to zeta?
                float zeta = std::max(0.0f, factor*viscosity + 1);
                const float tau = 0.01;     // time constant in seconds
                float k = m / (tau*tau);    // spring constant in [N/m] = [kg/s^2]
                float mu = 2 * std::sqrt(m*k) * zeta;

                for (int i = 0; i < n; ++i)
                    step(et, targetPos, mu, k);

                return particle;
            }
        };
    }
}
