/*
    nucleus_engine.hpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once
#include <algorithm>
#include "sapphire_engine.hpp"

namespace Sapphire
{
    inline PhysicsVector EffectiveVelocity(const PhysicsVector& vel, float speedLimit)
    {
        float rawSpeed = Magnitude(vel);
        if (rawSpeed < speedLimit / 1.0e+6)
            return vel;     // prevent division by zero, or by very small numbers

        float effSpeed = BicubicLimiter(rawSpeed, speedLimit);
        return (effSpeed/rawSpeed) * vel;
    }

    struct Particle
    {
        PhysicsVector pos;
        PhysicsVector vel;
        PhysicsVector force;
        float mass = 1.0e-3f;

        bool isFinite() const
        {
            // We don't check `force` because it is a temporary part of the calculation.
            // Any problems in `force` will show up in `pos` and `vel`.
            return pos.isFinite3d() && vel.isFinite3d();
        }
    };

    using NucleusDcRejectFilter = StagedFilter<float, 3>;

    class NucleusEngine
    {
    private:
        const float max_dt = 0.0005f;
        std::vector<Particle> curr;
        std::vector<Particle> next;
        float magneticCoupling = 0.0f;
        float speedLimit = 1000.0f;
        AutomaticGainLimiter agc;
        bool enableAgc = false;
        int fixedOversample = 0;                // 0 = calculate oversample, >0 = specify oversampling count
        std::vector<float> outputBuffer;        // allows feeding output data through the Automatic Gain Limiter.
        float aetherSpin = 0;
        float aetherVisc = 0;

        // DC reject state (consider moving into a separate class...)
        bool enableDcReject = false;
        const int crossfadeLimit = 8000;
        int crossfadeCounter{};
        float mixFilt{};
        std::vector<NucleusDcRejectFilter> filterArray;     // 3 filters per particle: (x, y, z)
        bool filtersNeedReset = false;

        void calculateForces(std::vector<Particle>& array)
        {
            // FIXFIXFIX: include aetherSpin, aetherVisc in the force calculations: a kind of "frame dragging".

            const float overlapDistance = 1.0e-4f;
            const int n = static_cast<int>(numParticles());

            // Reset all forces to zero, preparing to tally them.
            for (Particle& p : array)
                p.force = PhysicsVector::zero();

            // Find each pair of distinct particles (a, b).
            // Add up all the forces acting between the particles.
            for (int i = 0; i+1 < n; ++i)
            {
                Particle& a = array.at(i);
                for (int j = i+1; j < n; ++j)
                {
                    Particle& b = array.at(j);

                    // Find the mutual force vector between the two vectors.
                    // This includes an electrostatic component and a magnetic component.

                    // Calculate mutual quadrature = distance squared.
                    PhysicsVector dr = b.pos - a.pos;
                    float dist2 = Quadrature(dr);

                    // If two particles are very close to each other, consider them overlapping,
                    // and consider there to be zero force between them. This prevents division by
                    // zero (or by very small distances), resulting in destabilizing the simulation.
                    if (dist2 > overlapDistance * overlapDistance)
                    {
                        float dist = std::sqrt(dist2);
                        float dist3 = dist2 * dist;

                        // Use effective velocities, not raw velocities, to calculate magnetic cross product.
                        PhysicsVector av = EffectiveVelocity(a.vel, speedLimit);
                        PhysicsVector bv = EffectiveVelocity(b.vel, speedLimit);

                        // Calculate the magnetic component of the mutual force and include it in the vector sum.
                        PhysicsVector f = (dist - 1/dist3)*dr + (magneticCoupling / dist3)*Cross(bv - av, dr);

                        // Forces always act in equal and opposite pairs.
                        a.force += f;
                        b.force -= f;
                    }
                }
            }
        }

        void extrapolate(float dt)
        {
            const int n = static_cast<int>(numParticles());

            for (int i = 0; i < n; ++i)
            {
                const Particle& p1 = curr.at(i);
                Particle& p2 = next.at(i);

                // F = m*a  ==>  a = F/m.
                PhysicsVector acc = p1.force / p1.mass;

                // Estimate the net velocity change over the interval.
                PhysicsVector dV = dt * acc;

                // Calculate new position using mean velocity change over the interval.
                // We must use the bicubic limiter / "effective velocity" to avoid explosions.

                PhysicsVector v2 = p1.vel + dV/2;
                p2.pos = p1.pos + (dt * EffectiveVelocity(v2, speedLimit));

                // Calculate the velocity at the end of the time interval.
                p2.vel = p1.vel + dV;
            }
        }

        void step(float dt, float friction)
        {
            const int n = static_cast<int>(numParticles());

            // Calculate the forces at the existing configuration.
            calculateForces(curr);

            // Do a naive extrapolation to the midpoint of the time interval.
            // We assume the resulting configuration closely approximates the mean
            // conditions over the whole time interval.
            extrapolate(dt / 2);

            // Calculate forces after moving halfway.
            calculateForces(next);

            // Pretend like the midpoint forces apply at the beginning of the time interval.
            for (int i = 0; i < n; ++i)
                curr[i].force = next[i].force;

            // Extrapolate to the full time interval.
            extrapolate(dt);

            // Update the current state to the calcuted next state.
            // Apply friction at the same time.
            for (int i = 0; i < n; ++i)
            {
                curr[i] = next[i];
                curr[i].vel *= friction;
            }
        }

        float filter(float sampleRate, int i, int k, float x)
        {
            if (mixFilt > 0)
            {
                // DC rejection is enabled, or we are crossfading.
                NucleusDcRejectFilter& filt = filterArray.at(3*i + k);
                float y = filtersNeedReset ? filt.SnapHiPass(x) : filt.UpdateHiPass(x, sampleRate);
                return (1-mixFilt)*x + mixFilt*y;
            }
            return x;
        }

    public:
        explicit NucleusEngine(std::size_t _nParticles)
            : curr(_nParticles)
            , next(_nParticles)
            , outputBuffer(3 * _nParticles)     // (x, y, z) position vectors
            , filterArray(3 * _nParticles)
        {
            initialize();
        }

        void initialize()
        {
            for (NucleusDcRejectFilter& f : filterArray)
            {
                f.SetCutoffFrequency(30);
                f.Reset();
            }
            crossfadeCounter = 0;
            enableFixedOversample(1);
            setAgcEnabled(true);
            setDcRejectEnabled(true);
            setAetherSpin();
            setAetherVisc();
            filtersNeedReset = true;     // anti-click measure: eliminate step function being fed through filters!

            // The caller is responsible for resetting particle states.
            // For example, the caller might want to call SetMinimumEnergy(engine) after calling this function.
        }

        void resetAfterCrash()      // called when infinite/NAN output is detected, to pop back into the finite world
        {
            filtersNeedReset = true;
            agc.initialize();

            const int n = static_cast<int>(numParticles());
            for (int i = 0; i < n; ++i)
                for (int k = 0; k < 3; ++k)
                    output(i, k) = 0;

            // The caller is responsible for resetting particle states.
            // For example, the caller might want to call SetMinimumEnergy(engine) after calling this function.
        }

        void enableFixedOversample(int n)
        {
            fixedOversample = std::max(1, n);
        }

        void enableAutomaticOversample()
        {
            fixedOversample = 0;
        }

        bool getAgcEnabled() const
        {
            return enableAgc;
        }

        void setAgcEnabled(bool enable)
        {
            if (enable && !enableAgc)
            {
                // If the AGC isn't enabled, and caller wants to enable it,
                // re-initialize the AGC so it forgets any previous level it had settled on.
                agc.initialize();
            }
            enableAgc = enable;
        }

        void setAgcLevel(float level)
        {
            agc.setCeiling(level);
        }

        float getAetherSpin() const
        {
            return aetherSpin;
        }

        void setAetherSpin(float s = 0)
        {
            aetherSpin = std::clamp(s, -1.0f, +1.0f);
        }

        float getAetherVisc() const
        {
            return aetherVisc;
        }

        void setAetherVisc(float v = 0)
        {
            aetherVisc = std::clamp(v, 0.0f, +1.0f);
        }

        double getAgcDistortion() const     // returns 0 when no distortion, or a positive value correlated with AGC distortion
        {
            return enableAgc ? (agc.getFollower() - 1.0) : 0.0;
        }

        bool getDcRejectEnabled() const
        {
            return enableDcReject;
        }

        void setDcRejectEnabled(bool enable)
        {
            if (enable != enableDcReject)
            {
                // Trigger a cross-fade, to prevent clicking in audio streams.
                enableDcReject = enable;
                crossfadeCounter = crossfadeLimit;

                // Force resetting filters when we process the first input sample.
                // This will make them "snap" to the initial DC state.
                if (enable)
                    filtersNeedReset = true;
            }
        }

        void setMagneticCoupling(float mc)
        {
            magneticCoupling = mc;
        }

        void update(float dt, float halflife, float sampleRate, float gain)
        {
            // Use oversampling to keep the time increment within stability limits.
            // Allow the caller to specify the exact oversampling rate, or we allow
            // the caller to let us figure it out for them (variable performance though).
            int n = fixedOversample;
            if (n < 1)
            {
                // Automatic adjustment of oversampling is enabled.
                n = static_cast<int>(std::ceil(dt / max_dt));
                if (n < 1) n = 1;   // should never happen, but be careful
            }

            // Iterate the model over each oversampled step.
            const double et = dt / n;
            const float friction = std::pow(0.5, static_cast<double>(et)/halflife);
            for (int i = 0; i < n; ++i)
                step(et, friction);

            // Prepare for any crossfading betweeen raw signals and DC rej signals.
            if (enableDcReject || (crossfadeCounter > 0))
            {
                // We will mix each DC-reject signal with the corresponding raw signal using a linear crossfade.
                mixFilt = static_cast<float>(crossfadeCounter) / static_cast<float>(crossfadeLimit);
                if (enableDcReject)
                    mixFilt = 1 - mixFilt;      // toggle the crossfade direction

                if (crossfadeCounter > 0)
                    --crossfadeCounter;
            }

            // Copy outputs, and apply optional DC rejection.
            // For a couple hundred samples after toggling the DC rejection option,
            // there is a linear crossfade between the raw signal and the filtered
            // signal, to reduce audio popping.
            const int nparticles = static_cast<int>(curr.size());
            for (int i = 0; i < nparticles; ++i)
            {
                const Particle& p = curr.at(i);
                for (int k = 0; k < 3; ++k)
                    output(i, k) = gain * filter(sampleRate, i, k, p.pos[k]);
            }

            if (mixFilt > 0)
                filtersNeedReset = false;       // the `filter` function has finished resetting all the filters by now

            if (enableAgc)
                agc.process(sampleRate, outputBuffer);
        }

        float& output(int p, int k)
        {
            return outputBuffer.at(3*p + k);
        }

        std::size_t numParticles() const
        {
            return curr.size();
        }

        Particle& particle(int index)
        {
            return curr.at(index);
        }

        const Particle& particle(int index) const
        {
            return curr.at(index);
        }
    };
}
