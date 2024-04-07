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


        class Engine
        {
        private:
            const float speedFactor = 100;
            const float max_dt = -1;
            ParticleState particle;

            static ParticleState extrapolate(ParticleState state, float acc, float dt, float friction)
            {
                float dv = dt * acc;
                float v2 = state.vel + dv;       // final velocity
                float vm = state.vel + dv/2;     // mean velocity over the interval
                return ParticleState(state.pos + dt*vm, v2*friction);
            }

            void step(float dt, float targetPos, float friction)
            {
                float acc1 = dt*(targetPos - particle.pos);      // Hooke's Law for spring force F = ma = kx
                ParticleState midpoint = extrapolate(particle, acc1, dt / 2, friction);

                float acc2 = dt*(targetPos - midpoint.pos);     // approximate the mean acceleration over the interval
                particle = extrapolate(particle, acc2, dt, friction);
            }

            float getHalfLife(float viscosity)
            {
                const int minExp = -1;
                const int maxExp = +2;
                const float exponent = maxExp - viscosity*(minExp - maxExp);
                return std::pow(10.0f, exponent);
            }

        public:
            void initialize()
            {
                particle.pos = 0;
                particle.vel = 0;
            }

            float process(float dt, float targetPos, float viscosity)
            {
                const int n = (max_dt <= 0.0) ? 1 : static_cast<int>(std::ceil(dt / max_dt));
                const double et = speedFactor * (dt / n);
                const float halflife = getHalfLife(viscosity);
                const float friction = std::pow(0.5, static_cast<double>(et)/halflife);
                for (int i = 0; i < n; ++i)
                    step(et, targetPos, friction);
                return particle.pos;
            }
        };
    }
}
