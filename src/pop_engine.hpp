#pragma once
#include <cmath>

namespace Sapphire
{
    namespace Pop
    {
        const float MIN_POP_SPEED = -7;
        const float MAX_POP_SPEED = +7;
        const float MEAN_POP_RATE_HZ = 2;    // matches VCV LFO default frequency in Hz

        enum class TriggerState
        {
            Waiting,    // counting down for next radioactive decay event
            Firing,     // fire for 1 millisecond
            Quiet,      // stay quiet for 1 millisecond after firing, before waiting again
        };

        class Engine
        {
        public:
            Engine()
            {
                initialize();
            }

            void initialize()
            {
            }

            float setSpeed(float s)
            {
                const float minSpeed = -7;
                const float maxSpeed = +7;
                speed = std::clamp(s, minSpeed, maxSpeed);
                return speed;
            }

            float setChaos(float c)
            {
                const float minChaos = 0;
                const float maxChaos = 1;
                chaos = std::clamp(c, minChaos, maxChaos);
                return chaos;
            }

            float process(float dt)
            {
                const float two = 2;
                const float et = dt * std::pow(two, speed);
                secondsRemaining -= et;

                switch (state)
                {
                case TriggerState::Waiting:
                default:
                    if (secondsRemaining <= 0)
                    {
                        // Time to fire a trigger!
                        state = TriggerState::Firing;
                        secondsRemaining = 0.001;
                        return 10;
                    }
                    return 0;

                case TriggerState::Firing:
                    if (secondsRemaining <= 0)
                    {
                        // Stop firing the trigger. Go to zero volts for another millisecond.
                        state = TriggerState::Quiet;
                        secondsRemaining = 0.001;
                        return 0;
                    }
                    return 10;

                case TriggerState::Quiet:
                    if (secondsRemaining <= 0)
                    {
                        state = TriggerState::Waiting;
                        secondsRemaining = nextWaitInterval();
                    }
                    return 0;
                }
            }

            float nextWaitInterval()
            {
                return 0.5;
            }

        private:
            float speed = 0;
            float chaos = 1;
            float secondsRemaining = 0;
            TriggerState state = TriggerState::Waiting;
        };
    }
}
