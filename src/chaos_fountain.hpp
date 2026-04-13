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


    template <unsigned nsignals, typename rand_t = std::mt19937_64>
    struct ChaosFountain     // produces an arbitrary number of distinct smooth random curves
    {
    private:
        std::uint64_t seed = 0;
        std::array<unsigned, nsignals> permutation{};

    public:
        // We produce any number of one-channel signals by using the equivalent
        // of several Sapphire Glee modules (Aizawa).
        // Calculate how many Aizawa triplet-generators we need by rounding up.
        static constexpr unsigned nTriplets = (nsignals + 2) / 3;

        std::array<Aizawa, nTriplets> oscillators{};

        using batch_t = ChaosBatch<nsignals>;

        std::uint64_t getSeed() const { return seed; }
        void forgetSeed() { seed = 0; }

        batch_t process(
            float sampleRateHz,
            float speedKnob,        // relative time-flow rate in octaves
            float levelKnob)        // how intense the chaos is across all attenuverters
        {
            batch_t batch;

            assert(seed != 0);      // must initialize the seed before generating random numbers

            const double dt = std::pow<double>(2.0, speedKnob) / sampleRateHz;
            static const double r = 1.0 / std::sqrt(3.0);
            static const double w = (r+1)/2;
            static const double u = (r-1)/2;

            unsigned n = 0;     // how many signals have been generated

            for (auto& osc : oscillators)
            {
                osc.update(dt, 1);

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
            if (seed != 0)
            {
                // Restart all chaotic oscillators deterministically based on the seed.
                rand_t gen(seed);
                randomizeChaoticOscillators(gen);
                shuffleSignalMapping(gen);
            }
        }

        void reset(uint64_t newSeed)
        {
            seed = newSeed;
            reset();
        }

    private:
        void append(unsigned& n, batch_t& batch, float signal)
        {
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
            unsigned bits = 0;

            bool alreadyPicked[256]{};

            for (auto& osc : oscillators)
            {
                if (bits < 2)
                {
                    accum = gen();
                    bits = 64;
                }
                osc.initialize();
                osc.setMode(0);     // Glee: Apple

                unsigned r = accum & 3;
                accum >>= 2;
                bits -= 2;
                osc.setKnob(ChaosKnobChoices.at(r));

                for (int loop = 0; true; ++loop)        // safety limit: prevent infinite loop
                {
                    if (bits < 8)
                    {
                        accum = gen();
                        bits = 64;
                    }
                    r = accum & 0xff;
                    accum >>= 8;
                    bits -= 8;
                    if (!alreadyPicked[r] || loop==3)
                    {
                        alreadyPicked[r] = true;
                        osc.setState(ChaosInitialStateTable.at(r));
                        break;
                    }
                }
            }
        }
    };
}
