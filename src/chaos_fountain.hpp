#pragma once
#include <cassert>
#include <array>
#include <cmath>
#include <random>
#include <chrono>
#include <cstdint>
#include <functional>
#include <utility>
#include "chaos.hpp"

namespace Sapphire
{
    extern const std::array<ChaoticOscillatorState, 256> ChaosInitialStateTable;
    extern const std::array<float, 4> ChaosKnobChoices;

    template <unsigned nsignals>
    struct ChaosBatch
    {
        std::array<float, nsignals> signal{};
    };

    constexpr uint64_t ChaosFountainDefaultSeed = 0x5361707068697265;   // ASCII "Sapphire"

    constexpr uint64_t FilterSeed(uint64_t seed)
    {
        return seed ? seed : ChaosFountainDefaultSeed;
    }

    // Compile-time unit tests with zero runtime overhead.
    static_assert(FilterSeed(0) == ChaosFountainDefaultSeed);
    static_assert(FilterSeed(0xa8a78dac6de3725e) == 0xa8a78dac6de3725e);

    template <unsigned nsignals, typename rand_t = std::mt19937_64>
    struct ChaosFountain     // produces an arbitrary number of distinct smooth random curves
    {
    private:
        std::uint64_t seed{};
        std::array<unsigned, nsignals> permutation{};

    public:
        explicit ChaosFountain(uint64_t initSeed)
        {
            reset(initSeed);
        }

        // We produce any number of one-channel signals by using the equivalent
        // of several Sapphire Glee modules (Aizawa).
        // Calculate how many Aizawa triplet-generators we need by rounding up.
        static constexpr unsigned nTriplets = (nsignals + 2) / 3;

        std::array<Aizawa, nTriplets> oscillators{};
        std::array<double, nTriplets> rateFactor{};

        using batch_t = ChaosBatch<nsignals>;

        std::uint64_t getSeed() const { return seed; }

        void update(double dt)
        {
            for (unsigned i = 0; i < nTriplets; ++i)
                oscillators[i].step(dt * rateFactor[i]);
        }

        batch_t getBatch(float levelKnob) const
        {
            batch_t batch;
            unsigned n = 0;     // how many signals have been generated

            static const double r = 1.0 / std::sqrt(3.0);
            static const double w = (r+1)/2;
            static const double u = (r-1)/2;
            for (const auto& osc : oscillators)
            {
                // The Aizawa attractor has a different average orbital period
                // for its z-component than for the x- and y-components.
                // The z value orbit takes about 4 times as long as x and y.
                // We prefer to generate 3 signals with overall similar behavior.
                // Solution: rotate the coordinate space along 3D diagonals,
                // so that the resulting data are combinations of (x, y, z).

                const double x = osc.xpos();
                const double y = osc.ypos();
                const double z = osc.zpos();

                append(n, batch, (x + y + z)*r);
                append(n, batch, x*w + y*u - z*r);
                append(n, batch, x*u + y*w - z*r);
            }

            assert(n == nsignals);

            for (float& x : batch.signal)
                x *= levelKnob;

            return batch;
        }

        void reset()
        {
            rand_t gen(seed);
            randomizeChaoticOscillators(gen);
            shuffleSignalMapping(gen);
            randomizeRates(gen);
        }

        void reset(uint64_t newSeed)
        {
            seed = FilterSeed(newSeed);
            reset();
        }

    private:
        void append(unsigned& n, batch_t& batch, float signal) const
        {
            // After we fill the batch, there will still be 0, 1, or 2 leftover values.
            // Silently discard these for the convenience of the caller.
            if (n < nsignals)
                batch.signal.at(permutation[n++]) = signal;
        }

        void shuffleSignalMapping(rand_t& gen)
        {
            for (unsigned i = 0; i < nsignals; ++i)
                permutation[i] = i;

            for (unsigned i = 1; i < nsignals; ++i)
                if (unsigned r = gen()%(i+1); r < i)
                    std::swap(permutation[r], permutation[i]);
        }

        void randomizeChaoticOscillators(rand_t& gen)
        {
            uint64_t accum = 0;
            unsigned nbits = 0;

            bool alreadyPicked[256]{};

            for (auto& osc : oscillators)
            {
                osc.initialize();
                osc.setMode(0);     // Glee: Apple

                if (nbits < 2)
                {
                    accum = gen();
                    nbits = 64;
                }
                unsigned r = accum & 3;
                accum >>= 2;
                nbits -= 2;
                osc.setKnob(ChaosKnobChoices.at(r));

                for (int loop = 0; true; ++loop)        // safety limit: prevent infinite loop
                {
                    if (nbits < 8)
                    {
                        accum = gen();
                        nbits = 64;
                    }
                    r = accum & 0xff;
                    accum >>= 8;
                    nbits -= 8;
                    if (!alreadyPicked[r] || loop==3)
                    {
                        alreadyPicked[r] = true;
                        osc.setState(ChaosInitialStateTable.at(r));
                        break;
                    }
                }
            }
        }

        void randomizeRates(rand_t& gen)
        {
            // Pick a number near 1, but allowed to randomly
            // stray from 1 to form a normal distribution,
            // but whose bell curve is clamped on the ends to
            // prevent straying more than one octave up or down.

            constexpr double mean = 1.0;
            constexpr double dev = 0.2;
            std::normal_distribution<double> dist(mean, dev);

            for (double& rate : rateFactor)
                rate = std::clamp<double>(dist(gen), 0.5,  2.0);
        }
    };
}
