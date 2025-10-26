#pragma once
#include "sapphire_engine.hpp"
#include "sapphire_random.hpp"
#include "sauce_engine.hpp"
#include "rk4_simulator.hpp"
#include "galaxy_engine.hpp"

namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nMobileParticles = 42;
        constexpr unsigned nParticles = 2 + nMobileParticles;    // one anchor at each end of the chain
        static_assert(nParticles > nMobileParticles);

        constexpr float horSpace = 0.01;    // horizontal spacing in meters
        constexpr float defaultStiffness = 89;

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

        struct VinaWire
        {
            struct channel_info_t
            {
                Gravy::SingleChannelGravyEngine<float> gravy;
                LoHiPassFilter<float> dcReject;
                LoHiPassFilter<float> pluckFilter;
                unsigned pluckIndex{};
            };

            unsigned pluckIndexBase[2] { 10, 19 };

            vina_sim_t sim;
            float gain{};
            float panLeftFactor{};
            float panRightFactor{};
            float speedFactor = 1;
            float targetSpeedFactor = 1;
            float decayHalfLife{};
            float releaseHalfLife{};
            bool isFirstSample = true;
            channel_info_t channelInfo[2];
            bool prevGate{};
            Galaxy::Engine reverb;
            RandomVectorGenerator rand;
            bool isReverbEnabled{};

            explicit VinaWire()
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

                q.pluckFilter.Reset();
                q.pluckFilter.SetCutoffFrequency(1400);

                q.pluckIndex = pluckIndexBase[channel];
            }

            void initialize()
            {
                isFirstSample = true;
                initChannel(0);
                initChannel(1);
                initReverb();
                speedFactor = targetSpeedFactor = 1;
                for (unsigned i = 0; i < nParticles; ++i)
                {
                    sim.state[i].pos = horSpace * i;
                    sim.state[i].vel = 0;
                }
                setPitch(0);
                setStiffness(defaultStiffness);
                setLevel();
                setPan();
                setDecay();
                setRelease();
                prevGate = false;
                isReverbEnabled = true;
            }

            void initReverb()
            {
                reverb.initialize();
                reverb.setReplace(0.906);
                reverb.setBrightness(0.628);
                reverb.setDetune(0.476);
                reverb.setBigness(0.0615);
                //reverb.setMix(0.163);     // MIX is initialized by setRelease() as a side-effect
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

            VinaStereoFrame stereoReverb(float sampleRateHz, float inLeft, float inRight)
            {
                float outLeft, outRight;
                reverb.process(sampleRateHz, inLeft, inRight, outLeft, outRight);
                return VinaStereoFrame(outLeft, outRight);
            }

            void updatePluckChannel(float sampleRateHz, bool trigger, unsigned channel, unsigned index)
            {
                channel_info_t& q = channelInfo[channel];
                float thump;
                if (trigger)
                {
                    constexpr float denom = 8;
                    thump = 1.7 + ((rand.next()-0.5) / denom);
                    q.pluckIndex = index;   // sample and hold on trigger
                }
                else
                {
                    thump = 0;
                }
                updateFilter(q.pluckFilter, sampleRateHz, thump);
                sim.state[q.pluckIndex].vel += q.pluckFilter.LoPass();
            }

            void updatePluck(float sampleRateHz, bool gate)
            {
                const bool trigger = (gate && !prevGate);
                unsigned r = rand.rand();
                unsigned leftOffset = r & 3;
                r >>= 2;
                unsigned rightOffset = r & 3;
                r >>= 2;
                if (r & 1)
                    std::swap(pluckIndexBase[0], pluckIndexBase[1]);
                updatePluckChannel(sampleRateHz, trigger, 0, pluckIndexBase[0] + leftOffset);
                updatePluckChannel(sampleRateHz, trigger, 1, pluckIndexBase[1] + rightOffset);
                prevGate = gate;
            }

            VinaStereoFrame update(float sampleRateHz, bool gate)
            {
                updatePluck(sampleRateHz, gate);

                constexpr float rho = 0.98;
                constexpr float tuning = 75.897;
                speedFactor = rho*speedFactor + (1-rho)*targetSpeedFactor;
                const float dt = tuning * (speedFactor / sampleRateHz);
                const unsigned oversample = std::max<unsigned>(1, static_cast<unsigned>(std::ceil(dt/max_dt)));
                const float et = dt / oversample;
                for (unsigned k = 0; k < oversample; ++k)
                    sim.step(et);
                brake(sampleRateHz, gate ? decayHalfLife : releaseHalfLife);
                constexpr float level = 1.0e+03;
                float rawLeft  = sim.state[32].pos;
                float rawRight = sim.state[34].pos;
                float left  = (level * gain * panLeftFactor ) * audioFilter(sampleRateHz, rawLeft,  0);
                float right = (level * gain * panRightFactor) * audioFilter(sampleRateHz, rawRight, 1);
                if (isReverbEnabled)
                {
                    VinaStereoFrame rvb = stereoReverb(sampleRateHz, left, right);
                    left  = rvb.sample[0];
                    right = rvb.sample[1];
                }
                if (!std::isfinite(left) || !std::isfinite(right))
                {
                    initialize();
                    return VinaStereoFrame(0, 0);
                }
                isFirstSample = false;
                return VinaStereoFrame(left, right);
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
                const float limit = voct + 2.25;
                channelInfo[0].gravy.setFrequency(limit);
                channelInfo[1].gravy.setFrequency(limit);
            }

            void setStiffness(float stiff)
            {
                sim.deriv.k = 1000 * stiff;      // stiffness/mass
            }

            static float decay(float knob)
            {
                constexpr float minExponent = -2.9;
                constexpr float maxExponent = +1.1;
                const float k = std::clamp<float>(knob, 0, 1);
                const float exponent = (1-k)*minExponent + k*maxExponent;
                return TenToPower(exponent);
            }

            void setLevel(float knob = 1)
            {
                float k = std::clamp<float>(knob, 0, 2);
                gain = Cube(k);
            }

            void setPan(float knob = 0)
            {
                const float k = std::clamp<float>(knob, -1, +1);
                const float theta = M_PI_4 * (k+1);       // map [-1, +1] onto [0, pi/2]
                panLeftFactor  = M_SQRT2 * std::cos(theta);
                panRightFactor = M_SQRT2 * std::sin(theta);
            }

            void setDecay(float knob = 0.5)
            {
                decayHalfLife = decay(knob);
            }

            void setRelease(float knob = 0.5)
            {
                const float k = std::clamp<float>(knob, 0, 1);
                const float u = k - 0.5f;

                // Vina's RELEASE controls both the release decay coefficient
                // and the reverb mix.

                constexpr float weakness = 1;
                const float weakKnob = u*weakness + 0.5;
                releaseHalfLife = decay(weakKnob) / 8;

                reverb.setMix(0.163 * (1 + 2*u));
            }
        };
    }
}
