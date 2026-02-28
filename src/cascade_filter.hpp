#pragma once
#include <array>
#include <cmath>
#include "sauce_engine.hpp"

namespace Sapphire
{
    namespace Empath
    {
        template <typename value_t, unsigned MAX_FILTER_STAGES>
        class CascadeFilter
        {
            static_assert(MAX_FILTER_STAGES > 0);

        public:
            using result_t = FilterResult<value_t>;

        private:
            using filter_t = Gravy::SingleChannelGravyEngine<value_t>;
            std::array<filter_t, MAX_FILTER_STAGES> stage;

        public:
            void initialize()
            {
                for (filter_t& f : stage)
                    f.initialize();
            }

            result_t process(float sampleRateHz, const value_t inSample, float cascade)
            {
                std::array<result_t, 1+MAX_FILTER_STAGES> result;

                // The first interpolation waypoint is the original dry audio.
                result[0] = inSample;

                // The remaining interpolation waypoints are successive stages of
                // feeding one filter's output into the next filter's input.
                for (unsigned i = 0; i < MAX_FILTER_STAGES; ++i)
                    result[i+1] = stage[i].process(sampleRateHz, result[i]);

                // Clamp extreme cases.

                if (!std::isfinite(cascade))
                    return result[0];

                if (cascade <= 0)
                    return result[0];

                if (cascade >= static_cast<float>(MAX_FILTER_STAGES))
                    return result[MAX_FILTER_STAGES];

                // Return the linear interpolation between the outputs of the adjacent stages.
                unsigned k = static_cast<unsigned>(std::floor(cascade));
                float m = cascade - k;
                return (1-m)*result.at(k) + m*result.at(k+1);
            }
        };
    }
}
