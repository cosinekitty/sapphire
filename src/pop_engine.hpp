#pragma once
#include <algorithm>
#include <cmath>
#include <ctime>

namespace Sapphire
{
    namespace Pop
    {
        const double MEAN_POP_RATE_HZ = 2;    // matches VCV LFO default frequency in Hz

        const double MIN_POP_SPEED = -7;
        const double MAX_POP_SPEED = +7;
        const double DEFAULT_POP_SPEED = 0;

        const double MIN_POP_CHAOS = 0;
        const double MAX_POP_CHAOS = 1;
        const double DEFAULT_POP_CHAOS = 0.5;

        enum class TriggerState
        {
            Reset,      // first-time initialization needed for random generator
            Waiting,    // counting down for next radioactive decay event
            Firing,     // fire for 1 millisecond
            Quiet,      // stay quiet for 1 millisecond after firing, before waiting again
        };

        enum class OutputMode
        {
            Trigger,
            Gate,
        };

        class Engine
        {
        public:
            bool sendTriggerOnReset = false;

            Engine()
            {
                initialize();
            }

            void initialize()
            {
                secondsRemaining = 0;
                state = TriggerState::Reset;
                gateVoltage = 0;
            }

            void setRandomSeed(unsigned rs)
            {
                randomSeed = rs;
            }

            double setSpeed(double knob)
            {
                speed = std::clamp(knob, MIN_POP_SPEED, MAX_POP_SPEED);
                return speed;
            }

            double setChaos(double knob)
            {
                const double x = std::clamp(knob, MIN_POP_CHAOS, MAX_POP_CHAOS);
                chaos = (x*x*x);
                return x;
            }

            OutputMode getOutputMode() const
            {
                return outputMode;
            }

            void setOutputMode(OutputMode mode)
            {
                outputMode = mode;
            }

            float process(double sampleRate)
            {
                const double dt = 1 / sampleRate;
                float triggerVoltage = 0;

                switch (state)
                {
                case TriggerState::Reset:
                default:
                    // Start deterministically, but with a different random seed for each channel.
                    gen.seed(randomSeed);

                    if (sendTriggerOnReset)
                    {
                        // Start out firing a trigger. Subsequent triggers have adjustably random timing.
                        state = TriggerState::Firing;
                        secondsRemaining = 0.001 - 1/sampleRate;
                        triggerVoltage = 10;
                        gateVoltage = 10 - gateVoltage;
                    }
                    else
                    {
                        // Start out waiting for the first random time interval.
                        state = TriggerState::Waiting;
                        secondsRemaining = nextWaitInterval();
                        triggerVoltage = 0;
                    }
                    break;

                case TriggerState::Waiting:
                    secondsRemaining -= dt * std::pow(2.0, static_cast<double>(speed));
                    if (secondsRemaining <= 0)
                    {
                        // Time to fire a trigger!
                        state = TriggerState::Firing;
                        secondsRemaining = 0.001 - 1/sampleRate;
                        triggerVoltage = 10;
                        gateVoltage = 10 - gateVoltage;
                    }
                    else
                        triggerVoltage = 0;
                    break;

                case TriggerState::Firing:
                    secondsRemaining -= dt;
                    if (secondsRemaining <= 0)
                    {
                        // Stop firing the trigger. Go to zero volts for another millisecond.
                        state = TriggerState::Quiet;
                        secondsRemaining = 0.001 - 1/sampleRate;
                        triggerVoltage = 0;
                    }
                    else
                        triggerVoltage = 10;
                    break;

                case TriggerState::Quiet:
                    secondsRemaining -= dt;
                    if (secondsRemaining <= 0)
                    {
                        // We have been quiet for 1 millisecond. Go back to the waiting state.
                        state = TriggerState::Waiting;
                        // Figure out how many seconds to wait for the next radioactive decay.
                        // Deduct the 2-millisecond trigger cycle (1ms @ 10V, 1ms @ 0V).
                        secondsRemaining = nextWaitInterval() - 0.002;
                    }
                    triggerVoltage = 0;
                    break;
                }

                switch (outputMode)
                {
                case OutputMode::Gate:
                    return gateVoltage;

                case OutputMode::Trigger:
                default:
                    return triggerVoltage;
                }
            }

            double nextWaitInterval()
            {
                return (1-chaos)/MEAN_POP_RATE_HZ + chaos*generateDeltaT(MEAN_POP_RATE_HZ);
            }

            double generateDeltaT(double lambda)
            {
                double u = dis(gen);
                double dt = -std::log(u) / lambda;
                return dt;
            }

        private:
            OutputMode outputMode = OutputMode::Trigger;
            float gateVoltage = 0;
            unsigned randomSeed = 8675309;
            double speed = DEFAULT_POP_SPEED;
            double chaos = DEFAULT_POP_CHAOS;
            double secondsRemaining = 0;
            TriggerState state = TriggerState::Reset;
            std::mt19937 gen;
            std::uniform_real_distribution<double> dis{0.001, 1.0};
        };
    }
}
