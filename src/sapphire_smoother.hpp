#pragma once
#include <functional>
#include <cassert>

namespace Sapphire
{
    class Smoother
    {
    private:
        enum class State { Stable, Fading, Ramping };

        State state;
        double gain;
        const double rampSeconds;

        void Smoother_initialize()
        {
            state = State::Stable;
            gain = 1;
        }

    public:
        virtual void onSilent() {}   // called once at the silence in the middle of the ducking period
        virtual void onStable() {}   // called when we are stable again after ducking

        explicit Smoother(double rampTimeInSeconds = 0.005)
            : rampSeconds(rampTimeInSeconds)
        {
            Smoother_initialize();
        }

        virtual void initialize()
        {
            Smoother_initialize();
        }

        bool isStable() const
        {
            return state == State::Stable;
        }

        void fire()
        {
            state = State::Fading;
        }

        virtual double process(double sampleRateHz)
        {
            if (state == State::Stable)
                return 1;

            const double change = 1/(rampSeconds*sampleRateHz);
            if (state == State::Fading)
            {
                gain = std::clamp<double>(gain - change, 0, 1);
                if (gain == 0)
                {
                    state = State::Ramping;
                    onSilent();
                }
            }
            else // state == SmootherState::Ramping
            {
                gain = std::clamp<double>(gain + change, 0, 1);
                if (gain == 1)
                {
                    state = State::Stable;
                    onStable();
                }
            }

            return gain;
        }
    };
}
