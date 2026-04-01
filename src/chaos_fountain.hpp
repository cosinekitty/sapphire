#pragma once
#include <cassert>
#include <array>
#include <cmath>
#include <random>
#include <chrono>
#include <cstdint>
#include "chaos.hpp"

namespace Sapphire
{
    template <unsigned nsignals>
    struct ChaosBatch
    {
        std::array<float, nsignals> signal{};
    };


    template <unsigned nsignals>
    struct ChaosFountain     // produces an arbitrary number of distinct smooth random curves
    {
        // We produce any number of one-channel signals by using the equivalent
        // of several Sapphire Glee modules (Aizawa).
        // Calculate how many Aizawa triplet-generators we need by rounding up.
        static constexpr unsigned nTriplets = (nsignals + 2) / 3;

        std::array<Aizawa, nTriplets> oscillators{};
        std::uint64_t seed = 0;     // IMPORTANT: this is the value to save/load in your application.

        using batch_t = ChaosBatch<nsignals>;

        void commitSeed()
        {
            using namespace std;

            if (seed != 0)
                return;     // we already picked a seed, and once picked, we don't change it.

            // Generate a high-precision timestamp as a pseudorandom seed.
            const auto now = chrono::high_resolution_clock::now();
            seed = chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count();
            assert(seed != 0);

            // The seed is now valid for use to generate a series of
            // determinsitic and pseudorandon numbers.
            reset();
        }

        batch_t process(float speedKnob, float sampleRateHz)
        {
            commitSeed();

            batch_t batch;

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

                if (n < nsignals)
                    batch.signal[n++] = (x + y + z)*r;

                if (n < nsignals)
                    batch.signal[n++] = x*w + y*u - z*r;

                if (n < nsignals)
                    batch.signal[n++] = x*u + y*w - z*r;
            }

            assert(n == nsignals);
            return batch;
        }

        void reset()
        {
            using namespace std;

            // Restart all chaotic oscillators deterministically based on the seed.
            assert(seed != 0);

            mt19937 gen(seed);
            normal_distribution<float> dist(-3, +3);
            ChaoticOscillatorState state;

            for (auto& osc : oscillators)
            {
                osc.initialize();
                state.x = dist(gen);
                state.y = dist(gen);
                state.z = dist(gen);
                //osc.setState(state);
            }
        }
    };
}
