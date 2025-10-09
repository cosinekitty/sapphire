#pragma once
#include <cassert>
#include "sapphire_engine.hpp"
#include "sauce_engine.hpp"
#include "rk4_simulator.hpp"
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nMobileParticles = 42;
        constexpr unsigned nParticles = 2 + nMobileParticles;    // one anchor at each end of the chain
        static_assert(nParticles > nMobileParticles);

        constexpr float horSpace = 0.01;    // horizontal spacing in meters
        constexpr float verSpace = 0.01;    // vertical spacing in meters

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

        constexpr float max_dt = 0.004;

        struct VinaEngine
        {
            struct channel_info_t
            {
                Gravy::SingleChannelGravyEngine<float> gravy;
                LoHiPassFilter<float> dcReject;
            };

            vina_sim_t sim;
            float speedFactor = 1;
            float targetSpeedFactor = 1;
            bool isFirstSample = true;
            channel_info_t channelInfo[2];
            LoHiPassFilter<float> pluckFilter;
            float thump = 0;

            explicit VinaEngine()
                : sim(VinaDeriv(), nParticles)
                {}

            void initChannel(unsigned channel)
            {
                auto& q = channelInfo[channel];

                q.gravy.initialize();
                q.gravy.setFrequency(0.0);
                q.gravy.setResonance(0.2);
                q.gravy.setMix(0.8);
                q.gravy.setGain(0.5);

                q.dcReject.Reset();
                q.dcReject.SetCutoffFrequency(10);
            }

            void initialize()
            {
                isFirstSample = true;
                initChannel(0);
                initChannel(1);
                speedFactor = targetSpeedFactor = 1;
                for (unsigned i = 0; i < nParticles; ++i)
                {
                    sim.state[i].pos = PhysicsVector{horSpace*i, 0, 0, 0};
                    sim.state[i].vel = PhysicsVector{0, 0, 0, 0};
                }
                pluckFilter.Reset();
                pluckFilter.SetCutoffFrequency(2000);
            }

            void updateFilter(LoHiPassFilter<float>& filter, float sampleRateHz, float sample)
            {
                if (isFirstSample)
                    filter.Snap(sample);
                else
                    filter.Update(sample, sampleRateHz);
            }

            float audioFilter(float sampleRateHz, float sample, unsigned channel)
            {
                auto& q = channelInfo[channel];
                updateFilter(q.dcReject, sampleRateHz, sample);
                float audio = q.dcReject.HiPass();
                return q.gravy.process(sampleRateHz, audio).lowpass;
            }

            void pluck(float sampleRateHz)
            {
                thump = 1.7;
            }

            VinaStereoFrame update(float sampleRateHz, bool gate)
            {
                updateFilter(pluckFilter, sampleRateHz, thump);
                thump = 0;
                sim.state[3].vel[0] += pluckFilter.LoPass();

                constexpr float rho = 0.98;
                speedFactor = rho*speedFactor + (1-rho)*targetSpeedFactor;
                const float dt = 76.0808 * (speedFactor / sampleRateHz);
                const unsigned oversample = std::max<unsigned>(1, static_cast<unsigned>(std::ceil(dt/max_dt)));
                const float et = dt / oversample;
                for (unsigned k = 0; k < oversample; ++k)
                    sim.step(et);
                const float halfLifeSeconds = gate ? 1.0 : 0.045;
                brake(sampleRateHz, halfLifeSeconds);
                constexpr float level = 1.0e+03;
                float rawLeft  = sim.state[37].pos[0];
                float rawRight = sim.state[38].pos[0];
                if (!std::isfinite(rawLeft) || !std::isfinite(rawRight))
                {
                    // [4.372 warn src/vina_rk4.hpp:142 update] Vina auto-reset at dt=0.00503117
                    //WARN("Vina auto-reset at dt=%0.6g", dt);
                    initialize();
                    rawLeft = 0;
                    rawRight = 0;
                }
                float left  = level * audioFilter(sampleRateHz, rawLeft,  0);
                float right = level * audioFilter(sampleRateHz, rawRight, 1);
                isFirstSample = false;
                return VinaStereoFrame {left, right};
            }

            void brake(float sampleRateHz, float halfLifeSeconds)
            {
                const float factor = static_cast<float>(
                    std::pow<double>(0.5, 1.0/(sampleRateHz*halfLifeSeconds))
                );

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
                targetSpeedFactor = pow(2.0, voct);
                const float limit = voct + 2.9;
                channelInfo[0].gravy.setFrequency(limit);
                channelInfo[1].gravy.setFrequency(limit);
            }
        };
    }
}
