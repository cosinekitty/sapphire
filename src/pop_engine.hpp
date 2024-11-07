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
                updatePower();
                initialize();
            }

            void initialize()
            {
                state = TriggerState::Waiting;
                gateVoltage = 0;
                needReset = true;
            }

            void setRandomSeed(unsigned rs)
            {
                randomSeed = rs;
            }

            double setSpeed(double knob)
            {
                speed = std::clamp(knob, MIN_POP_SPEED, MAX_POP_SPEED);
                updatePower();
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
                const double et = dt * power;
                int triggerVoltage = 0;
                bool startFiringTrigger = false;

                remainingTime -= et;

                if (needReset)
                {
                    needReset = false;
                    gen.seed(randomSeed);
                    if (sendTriggerOnReset)
                    {
                        startFiringTrigger = true;
                    }
                    else
                    {
                        state = TriggerState::Waiting;
                        remainingTime = nextWaitInterval();
                    }
                }
                else if (state == TriggerState::Waiting)
                {
                    // If we have been waiting long enough, fire the next trigger.
                    if (remainingTime <= 0.0)
                        startFiringTrigger = true;
                }

                if (startFiringTrigger)
                {
                    gateVoltage = 10 - gateVoltage;
                    remainingTime = nextWaitInterval();
                    state = TriggerState::Firing;
                    beginMillisecondCountdown(sampleRate);
                }

                switch (state)
                {
                case TriggerState::Waiting:
                default:
                    // Do nothing until trigger firing logic above kicks us out of the waiting state.
                    break;

                case TriggerState::Firing:
                    if (pulseSamplesRemaining > 0)
                    {
                        --pulseSamplesRemaining;
                        triggerVoltage = 10;
                    }
                    else
                    {
                        state = TriggerState::Quiet;
                        beginMillisecondCountdown(sampleRate);
                    }
                    break;

                case TriggerState::Quiet:
                    if (pulseSamplesRemaining > 0)
                        --pulseSamplesRemaining;
                    else
                        state = TriggerState::Waiting;
                    break;
                }

                switch (outputMode)
                {
                case OutputMode::Gate:
                    return static_cast<float>(gateVoltage);

                case OutputMode::Trigger:
                default:
                    return static_cast<float>(triggerVoltage);
                }
            }

private:
            void beginMillisecondCountdown(double sampleRate)
            {
                pulseSamplesRemaining = static_cast<int>(std::round(sampleRate / 1000));
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

            void updatePower()
            {
                if (speed != prevSpeed)
                {
                    prevSpeed = speed;
                    power = std::pow(static_cast<double>(2), speed);
                }
            }

            OutputMode outputMode = OutputMode::Trigger;
            int gateVoltage = 0;
            unsigned randomSeed = 8675309;
            double prevSpeed = 99;
            double speed = DEFAULT_POP_SPEED;
            double power{};
            double remainingTime = 0;
            int pulseSamplesRemaining = 0;
            double chaos = DEFAULT_POP_CHAOS;
            TriggerState state = TriggerState::Waiting;
            bool needReset = true;
            std::mt19937 gen;
            std::uniform_real_distribution<double> dis{0.001, 1.0};
        };
    }
}
