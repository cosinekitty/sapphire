#pragma once
#include <cassert>
#include <array>
#include <cmath>
#include "chaos.hpp"

namespace Sapphire
{
    template <unsigned nsignals>
    struct ChaosBatch
    {
        std::array<float, nsignals> signal{};
    };


    template <unsigned nsignals>
    class ChaosFountain
    {
    private:
        using osc_t = Aizawa;       // same as used by Sapphire Glee
        static constexpr unsigned nosc = (nsignals + 2) / 3;

        std::array<osc_t, nosc> osc;

    public:
        using batch_t = ChaosBatch<nsignals>;

        void initialize()
        {
            for (auto& o : osc)
                o.initialize();
        }

        batch_t process(float speedKnob, float sampleRateHz)
        {
            batch_t batch;

            const double dt = std::pow<double>(2.0, speedKnob) / sampleRateHz;
            const double r = 1.0 / std::sqrt(3.0);
            const double w = (r+1)/2;
            const double u = (r-1)/2;

            unsigned n = 0;     // how many signals have been generated

            for (osc_t& o : osc)
            {
                o.update(dt, 1);

                // The Aizawa attractor has a different average orbital period
                // for its z-component than for the x- and y-components.
                // The z value orbit takes about 4 times as long as x and y.
                // We prefer to generate 3 signals with overall similar behavior.
                // Solution: rotate the coordinate space along 3D diagonals,
                // so that the resulting data are combinations of (x, y, z).
                const double x = o.xpos();
                const double y = o.ypos();
                const double z = o.zpos();

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
    };
}
