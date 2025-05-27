#pragma once
#include <algorithm>
#include <cmath>
#include <ctime>
#include "sapphire_engine.hpp"

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
                isFiringTrigger = false;
                gateVoltage = 0;
                needReset = true;
                pulseSamplesRemaining = 0;
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
                        isFiringTrigger = false;
                        remainingTime = nextWaitInterval();
                    }
                }
                else
                {
                    // If we have been waiting long enough, fire the next trigger.
                    // Tolerate a tiny amount of floating point error so that we don't
                    // lag by a random sample here and there.
                    startFiringTrigger = (!isFiringTrigger && (remainingTime <= 1.0e-6));
                }

                if (startFiringTrigger)
                {
                    gateVoltage = 10 - gateVoltage;
                    remainingTime = nextWaitInterval();
                    isFiringTrigger = true;
                    pulseSamplesRemaining = 1 + static_cast<int>(std::round(sampleRate / 1000));
                }

                if (isFiringTrigger)
                {
                    if (pulseSamplesRemaining > 0)
                    {
                        --pulseSamplesRemaining;
                        triggerVoltage = 10;
                    }
                    else
                    {
                        isFiringTrigger = false;
                    }
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
                    power = TwoToPower(speed);
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
            bool isFiringTrigger = false;
            bool needReset = true;
            std::mt19937 gen;
            std::uniform_real_distribution<double> dis{0.001, 1.0};
        };
    }
}
