#pragma once

namespace Sapphire
{
    class GateTriggerReceiver
    {
    private:
        float prevVoltage{};
        bool gate{};
        bool trigger{};
        bool fallingEdge{};

    public:
        bool isGateActive() const { return gate; }
        bool isTriggerActive() const { return trigger; }
        bool isFallingEdge() const { return fallingEdge; }

        void initialize()
        {
            prevVoltage = 0;
            gate = false;
            trigger = false;
            fallingEdge = false;
        }

        void update(float voltage)
        {
            trigger = false;
            fallingEdge = false;

            if (prevVoltage < 1 && voltage >= 1)
            {
                trigger = !gate;
                gate = true;
            }
            else if (prevVoltage >= 0.1 && voltage < 0.1)
            {
                fallingEdge = gate;
                gate = false;
            }

            prevVoltage = voltage;
        }
    };
}
