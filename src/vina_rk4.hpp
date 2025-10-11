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

        struct VinaStereoFrame
        {
            float sample[2];

            explicit VinaStereoFrame(float left, float right)
                : sample{left, right}
                {}
        };

        template <typename batch_t>
        struct ParticleBatch
        {
            batch_t pos;
            batch_t vel;

            explicit ParticleBatch()
                {}

            explicit ParticleBatch(batch_t _pos, batch_t _vel)
                : pos(_pos)
                , vel(_vel)
                {}

            friend ParticleBatch operator * (float k, const ParticleBatch& p)
            {
                return ParticleBatch(k*p.pos, k*p.vel);
            }

            friend ParticleBatch operator + (const ParticleBatch& a, const ParticleBatch& b)
            {
                return ParticleBatch(a.pos + b.pos, a.vel + b.vel);
            }
        };

        using VinaParticle = ParticleBatch<float>;
        using vina_state_t = std::vector<VinaParticle>;
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
            bool prevGate{};

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
                    sim.state[i].pos = horSpace * i;
                    sim.state[i].vel = 0;
                }
                pluckFilter.Reset();
                pluckFilter.SetCutoffFrequency(4000);
                setPitch(0);
                prevGate = false;
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

            VinaStereoFrame update(float sampleRateHz, bool gate, float leftAudioIn, float rightAudioIn)
            {
                const float thump = (gate && !prevGate) ? 1.7f : 0.0f;
                prevGate = gate;

                updateFilter(pluckFilter, sampleRateHz, thump);
                sim.state[10].vel += pluckFilter.LoPass();
                sim.state[14].vel += leftAudioIn;
                sim.state[18].vel += rightAudioIn;

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
                float rawLeft  = sim.state[32].pos;
                float rawRight = sim.state[38].pos;
                if (!std::isfinite(rawLeft) || !std::isfinite(rawRight))
                {
                    initialize();
                    return VinaStereoFrame{0, 0};
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
