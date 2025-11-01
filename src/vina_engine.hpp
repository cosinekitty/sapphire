#pragma once
#include "sapphire_engine.hpp"
#include "sapphire_random.hpp"
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
        constexpr float defaultStiffness = 89;
        constexpr float defaultChorusHz = 0.2;

        struct VinaStereoFrame
        {
            float sample[2]{};

            VinaStereoFrame() {}

            explicit VinaStereoFrame(float left, float right)
                : sample{left, right}
                {}

            friend VinaStereoFrame operator* (float k, const VinaStereoFrame& f)
            {
                return VinaStereoFrame(k*f.sample[0], k*f.sample[1]);
            }

            friend VinaStereoFrame operator+ (const VinaStereoFrame& a, const VinaStereoFrame& b)
            {
                return VinaStereoFrame(a.sample[0] + b.sample[0], a.sample[1] + b.sample[1]);
            }
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
        constexpr unsigned delayLineFrames = 48000;     // up to 1 second at typical rate, less at higher rates.
        using chorus_delay_t = DelayLine<VinaStereoFrame, delayLineFrames>;

        constexpr float max_dt = 0.004;
        constexpr float inputScale = 1000;
        constexpr float outputScale = 1000 / inputScale;

        struct VinaWire
        {
            struct channel_info_t
            {
                Gravy::SingleChannelGravyEngine<float> gravy;
                LoHiPassFilter<float> dcReject;
                LoHiPassFilter<float> pluckFilter;
                unsigned pluckIndex{};
            };

            enum class RenderState
            {
                Playing,
                RampingDown,
                Quiet,
            };

            unsigned pluckIndexBase[2] { 10, 19 };

            vina_sim_t sim;
            float gain{};
            float speedFactor = 1;
            float targetSpeedFactor = 1;
            float decayHalfLife{};
            float releaseHalfLife{};
            channel_info_t channelInfo[2];
            bool prevGate{};
            RandomVectorGenerator rand;
            bool isStandbyEnabled{};
            unsigned renderSamples{};
            unsigned fadeSamples{};
            unsigned resetSamples{};
            RenderState renderState{};
            float originLeft{};
            float originRight{};
            float pan{};
            float spread{};
            float currentSpreadAngle{};
            float feedback{};
            bool isChorusEnabled = true;
            float chorusDepth{};
            float chorusRate{};
            float chorusAngle{};
            chorus_delay_t chorusDelay{};

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
                initChannel(0);
                initChannel(1);
                speedFactor = targetSpeedFactor = 1;
                for (unsigned i = 0; i < nParticles; ++i)
                {
                    sim.state[i].pos = horSpace * i;
                    sim.state[i].vel = 0;
                }
                originLeft  = leftParticlePos();
                originRight = rightParticlePos();
                setPitch(0);
                setStiffness(defaultStiffness);
                setLevel();
                setPan();
                setSpread();
                setDecay();
                setRelease();
                setFeedback();
                initChorus();
                prevGate = false;
                setStandbyEnabled(true);
                resetSamples = 0;
            }

            void initChorus()
            {
                setChorusDepth();
                setChorusRate();
                chorusAngle = 0;
                chorusDelay.clear();
            }

            void setStandbyEnabled(bool enable)
            {
                isStandbyEnabled = enable;
                renderState = enable ? RenderState::Quiet : RenderState::Playing;
                renderSamples = 0;
                fadeSamples = 0;
            }

            float leftParticlePos() const
            {
                return sim.state[32].pos;
            }

            float rightParticlePos() const
            {
                return sim.state[34].pos;
            }

            float leftParticleDisp() const
            {
                return outputScale * (leftParticlePos() - originLeft);
            }

            float rightParticleDisp() const
            {
                return outputScale * (rightParticlePos() - originRight);
            }

            float audioFilter(float sampleRateHz, float sample, unsigned channel)
            {
                auto& q = channelInfo[channel];
                q.dcReject.Update(sample, sampleRateHz);
                float audio = q.dcReject.HiPass();
                return q.gravy.process(sampleRateHz, audio).lowpass;
            }

            void updatePluckChannel(float sampleRateHz, bool trigger, unsigned channel, unsigned index)
            {
                channel_info_t& q = channelInfo[channel];
                float thump;
                if (trigger)
                {
                    constexpr float denom = 8;
                    thump = inputScale * (1.7 + ((rand.next()-0.5) / denom));
                    q.pluckIndex = index;   // sample and hold on trigger
                }
                else
                {
                    thump = 0;
                }
                q.pluckFilter.Update(thump, sampleRateHz);
                sim.state[q.pluckIndex].vel += q.pluckFilter.LoPass();
            }

            void updatePluck(float sampleRateHz, bool gate, bool trigger)
            {
                unsigned r = rand.rand();
                unsigned leftOffset = r & 3;
                r >>= 2;
                unsigned rightOffset = r & 3;
                r >>= 2;
                if (r & 1)
                    std::swap(pluckIndexBase[0], pluckIndexBase[1]);
                updatePluckChannel(sampleRateHz, trigger, 0, pluckIndexBase[0] + leftOffset);
                updatePluckChannel(sampleRateHz, trigger, 1, pluckIndexBase[1] + rightOffset);
            }

            bool updateTrigger(bool gate)
            {
                const bool trigger = (gate && !prevGate);
                prevGate = gate;
                return trigger;
            }

            VinaStereoFrame interpolateBackward(float sampleRateHz, float timeOffsetSec)
            {
                float s = std::max<float>(0, sampleRateHz * timeOffsetSec);
                unsigned s1 = static_cast<unsigned>(std::floor(s));
                float frac = s - s1;
                VinaStereoFrame f1 = chorusDelay.readBackward(s1);
                VinaStereoFrame f2 = chorusDelay.readBackward(s1+1);
                return (1-frac)*f1 + frac*f2;
            }

            VinaStereoFrame update(float sampleRateHz, bool gate)
            {
                if (resetSamples > 0)
                {
                    --resetSamples;
                    return VinaStereoFrame(0, 0);
                }

                const bool trigger = updateTrigger(gate);
                if (trigger)
                {
                    renderSamples = 0;
                    renderState = RenderState::Playing;
                    currentSpreadAngle = M_PI_4 * std::clamp<float>(rand.next()/2, -1, +1);
                }

                float theta = M_PI_4*(pan+1);       // map [-1, +1] onto [0, pi/2]
                theta = std::clamp<float>(spread*currentSpreadAngle + theta, 0, M_PI_2);
                float panLeftFactor  = M_SQRT2 * std::cos(theta);
                float panRightFactor = M_SQRT2 * std::sin(theta);

                updatePluck(sampleRateHz, gate, trigger);

                constexpr float rho = 0.98;
                constexpr float tuning = 75.845;
                speedFactor = rho*speedFactor + (1-rho)*targetSpeedFactor;

                float left = 0;
                float right = 0;
                if (renderState != RenderState::Quiet)
                {
                    // Oversample the RK4 simulation as needed to compensate for high pitch (speedFactor).
                    const float dt = tuning * (speedFactor / sampleRateHz);
                    const unsigned oversample = std::max<unsigned>(1, static_cast<unsigned>(std::ceil(dt/max_dt)));
                    const float et = dt / oversample;
                    for (unsigned k = 0; k < oversample; ++k)
                        sim.step(et);

                    brake(sampleRateHz, gate ? decayHalfLife : releaseHalfLife);

                    float rawLeft  = leftParticleDisp();
                    float rawRight = rightParticleDisp();
                    float power = std::hypotf(rawLeft, rawRight);

                    left  = (gain * panLeftFactor ) * audioFilter(sampleRateHz, rawLeft,  0);
                    right = (gain * panRightFactor) * audioFilter(sampleRateHz, rawRight, 1);
                    if (isStandbyEnabled)
                    {
                        if (renderState == RenderState::Playing)
                        {
                            constexpr float minPower = 0.0001;
                            if (power < minPower)
                            {
                                // See if we have an extended period of lower power.
                                ++renderSamples;
                                const unsigned timeoutSamples = static_cast<unsigned>(sampleRateHz/20);
                                if (renderSamples > timeoutSamples)
                                {
                                    renderState = RenderState::RampingDown;
                                    fadeSamples = static_cast<unsigned>(2 * sampleRateHz);
                                    renderSamples = fadeSamples;
                                }
                            }
                            else
                            {
                                // Something was loud enough to reset the low-power count.
                                renderSamples = 0;
                            }
                        }
                        else // (renderState == RenderState::RampingDown)
                        {
                            if (renderSamples > 0)
                            {
                                float fade = static_cast<float>(renderSamples) / static_cast<float>(fadeSamples);
                                left  *= fade;
                                right *= fade;
                                --renderSamples;
                            }
                            else
                            {
                                renderState = RenderState::Quiet;
                                renderSamples = 0;
                                left = right = 0;
                            }
                        }
                    }
                }

                // Keep the chorusDelay consistently fed, whether or not chorus is enabled.
                // The CPU cost is very small, and it prevents unwanted glitches.
                chorusDelay.write(VinaStereoFrame(left, right));
                if (isChorusEnabled && chorusDepth>0)
                {
                    // FIXFIXFIX: 3 phase-shifted copies of each voice, preserving (left, right) as a unit.
                    // Slowly modulate position in time using linear/sinc interpolation (like Echo).
                    constexpr float chorusMaxSeconds = 0.03;
                    constexpr float rateAdjust = 0.5;

                    // Calculate sample offsets into the past for the 3 chorus voices.
                    const float timeFactor = chorusDepth * (chorusMaxSeconds/2);
                    const float t0 = timeFactor*(1 - std::cos(chorusAngle));
                    const float t1 = timeFactor*(1 - std::cos(chorusAngle + (M_PI*2)/3));
                    const float t2 = timeFactor*(1 - std::cos(chorusAngle - (M_PI*2)/3));

                    VinaStereoFrame f0 = interpolateBackward(sampleRateHz, t0);
                    VinaStereoFrame f1 = interpolateBackward(sampleRateHz, t1);
                    VinaStereoFrame f2 = interpolateBackward(sampleRateHz, t2);

                    // Mix with the original using chorusDepth as the mix parameter.
                    left  = (1-chorusDepth)*left  + (chorusDepth/3)*(f0.sample[0] + f1.sample[0] + f2.sample[0]);
                    right = (1-chorusDepth)*right + (chorusDepth/3)*(f0.sample[1] + f1.sample[1] + f2.sample[1]);

                    // Update chorusAngle for next iteration.
                    const float power = TenToPower<float>(rateAdjust * chorusRate);
                    const float increment = power * (2*M_PI)*(defaultChorusHz/sampleRateHz);
                    chorusAngle = std::fmod(chorusAngle + increment, 2*M_PI);
                }

                if (!std::isfinite(left) || !std::isfinite(right))
                {
                    initialize();
                    left = right = 0;
                    resetSamples = static_cast<unsigned>(sampleRateHz / 2);
                }

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
                pan = std::clamp<float>(knob, -1, +1);
            }

            void setSpread(float knob = 0)
            {
                spread = std::clamp<float>(knob, 0, 1);
            }

            void setDecay(float knob = 0.5)
            {
                decayHalfLife = decay(knob);
            }

            void setRelease(float knob = 0.5)
            {
                const float k = std::clamp<float>(knob, 0, 1);
                const float u = k - 0.5f;
                constexpr float weakness = 1;
                const float weakKnob = u*weakness + 0.5;
                constexpr float scale = 0.17;
                releaseHalfLife = scale * decay(weakKnob);
            }

            void setFeedback(float knob = 0)
            {
                feedback = std::clamp<float>(knob, -1, +1);
            }

            void setChorusDepth(float knob = 0.5)
            {
                chorusDepth = std::clamp<float>(knob, 0, 1);
            }

            void setChorusRate(float knob = 0)
            {
                chorusRate = std::clamp<float>(knob, -1, +1);
            }
        };
    }
}
