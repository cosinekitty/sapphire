#pragma once

namespace Sapphire
{
    template <typename value_t>
    class FeedbackController
    {
    private:
        value_t vigor = 1;
        value_t inertia = 1;
        value_t accum = 0;

    public:
        void initialize()
        {
            accum = 0;
        }

        value_t process(value_t error, int sampleRateHz)
        {
            accum += error / (inertia * sampleRateHz);
            return vigor * (error + accum);
        }
    };
}
