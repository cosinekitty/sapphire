#pragma once
#include <cassert>
#include "sapphire_engine.hpp"
#include "sauce_engine.hpp"
#include "rk4_simulator.hpp"
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nMobileColumns = 42;
        constexpr unsigned nColumns = 44;
        constexpr unsigned nParticles = nColumns;
        constexpr unsigned nMobileParticles = nMobileColumns;
        static_assert(nParticles > nMobileParticles);
        constexpr unsigned nAnchorParticles = nParticles - nMobileParticles;

        constexpr float horSpace = 0.01;    // horizontal spacing in meters
        constexpr float verSpace = 0.01;    // vertical spacing in meters

        constexpr bool isMobileIndex(unsigned i)
        {
            return (i>0) && (i<=nMobileColumns);
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

            explicit VinaParticle()
                {}

            explicit VinaParticle(PhysicsVector _pos, PhysicsVector _vel)
                : pos(_pos)
                , vel(_vel)
                {}

            friend VinaParticle operator * (float k, const VinaParticle& p)
            {
                return VinaParticle(k*p.pos, k*p.vel);
            }

            friend VinaParticle operator + (const VinaParticle& a, const VinaParticle& b)
            {
                return VinaParticle(a.pos + b.pos, a.vel + b.vel);
            }
        };

        using vina_state_t = std::vector<VinaParticle>;
        extern const vina_state_t EngineInit;
    }
}

#include "vina_deriv.hpp"

namespace Sapphire
{
    namespace Vina
    {
        using vina_sim_t = RungeKutta::ListSimulator<float, VinaParticle, VinaDeriv>;

        struct VinaEngine
        {
            unsigned oversample = 1;
            float speedFactor = 1;
            float targetSpeedFactor = 1;
            Gravy::SingleChannelGravyEngine<float> gravy[2];
            LoHiPassFilter<float> dcReject[2];
            bool isFirstSample = true;

            explicit VinaEngine()
                : sim(VinaDeriv(), nParticles)
                {}

            void initChannel(unsigned channel)
            {
                auto& g = gravy[channel];
                g.initialize();
                g.setFrequency(0.65);
                g.setResonance(0.35);
                g.setMix(1);
                g.setGain(0.5);

                auto& d = dcReject[channel];
                d.Reset();
                d.SetCutoffFrequency(10);
            }

            void initialize()
            {
                isFirstSample = true;
                initChannel(0);
                initChannel(1);
                oversample = 1;
                speedFactor = targetSpeedFactor = 1;
                for (unsigned c = 0; c < nColumns; ++c)
                {
                    sim.state[c].pos = PhysicsVector{horSpace*c, 0, 0, 0};
                    sim.state[c].vel = PhysicsVector{0, 0, 0, 0};
                }
            }

            void pluck()
            {
                constexpr float thump = 1.7;
                sim.state[3].vel[0] += thump;
            }

            float filter(float sampleRateHz, float sample, unsigned channel)
            {
                if (isFirstSample)
                    dcReject[channel].Snap(sample);
                else
                    dcReject[channel].Update(sample, sampleRateHz);
                float audio = dcReject[channel].HiPass();
                return gravy[channel].process(sampleRateHz, audio).lowpass;
            }

            VinaStereoFrame update(float sampleRateHz, bool gate)
            {
                constexpr float rho = 0.98;
                speedFactor = rho*speedFactor + (1-rho)*targetSpeedFactor;
                const float dt = 76.0808 * (speedFactor / sampleRateHz);
                const float et = dt / oversample;
                for (unsigned k = 0; k < oversample; ++k)
                    sim.step(et);
                const float halfLifeSeconds = gate ? 1.0 : 0.045;
                brake(sampleRateHz, halfLifeSeconds);
                constexpr float level = 1.0e+03;
                float rawLeft  = sim.state[37].pos[0];
                float rawRight = sim.state[38].pos[0];
                float left  = level * filter(sampleRateHz, rawLeft,  0);
                float right = level * filter(sampleRateHz, rawRight, 1);
                isFirstSample = false;
                return VinaStereoFrame {left, right};
            }

            float maxSpeed() const
            {
                float s = 0;
                for (const VinaParticle& p : sim.state)
                    s = std::max(s, Quadrature(p.vel));
                return std::sqrt(s);
            }

            vina_sim_t sim;

            void brake(float sampleRateHz, float halfLifeSeconds)
            {
                const float factor = std::pow(0.5, 1/(sampleRateHz*halfLifeSeconds));
                for (VinaParticle& p : sim.state)
                    p.vel *= factor;
            }

            void settle(
                float sampleRateHz = 48000,
                float settleTimeSeconds = 5.0)
            {
                const unsigned nFrames = static_cast<unsigned>(sampleRateHz * settleTimeSeconds);
                for (unsigned frameCount = 0; frameCount < nFrames; ++frameCount)
                    update(sampleRateHz, false);
            }

            void setPreSettledState()
            {
                assert(sim.state.size() == EngineInit.size());
                sim.state = EngineInit;
            }

            void setPitch(float voct)
            {
                static constexpr float two = 2;     // work around silly compiler on MAC
                targetSpeedFactor = std::pow<float>(two, voct);
            }
        };
    }
}
