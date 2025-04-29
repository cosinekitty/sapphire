#pragma once
#include <functional>
#include <cassert>
#include "sapphire_vcvrack.hpp"
#include "sapphire_engine.hpp"

namespace Sapphire
{
    class Smoother
    {
    private:
        enum class State { Stable, Fading, Ramping };

        const double rampSeconds;

        State state;
        double gain;
        bool trigger;

        void Smoother_initialize()
        {
            state = State::Stable;
            gain = 1;
            trigger = false;
        }

    public:
        static constexpr double DefaultRampSeconds = 0.005;

        explicit Smoother(double _rampSeconds = DefaultRampSeconds)
            : rampSeconds(_rampSeconds)
        {
            Smoother_initialize();
        }

        virtual void onSilent() {}   // called once at the silence in the middle of the ducking period
        virtual void onStable() {}   // called when we are stable again after ducking

        virtual void initialize()
        {
            Smoother_initialize();
        }

        bool isStable() const
        {
            return state == State::Stable;
        }

        double getGain() const
        {
            return gain;
        }

        void begin()
        {
            state = State::Fading;
        }

        virtual bool changed() const
        {
            return false;
        }

        bool isDelayedActionReady() const
        {
            return trigger;
        }

        virtual double process(double sampleRateHz)
        {
            if (isStable())
            {
                if (!changed())
                    return gain;

                begin();
            }

            trigger = false;
            const double change = 1/(rampSeconds*sampleRateHz);
            if (state == State::Fading)
            {
                gain = std::clamp<double>(gain - change, 0, 1);
                if (gain == 0)
                {
                    state = State::Ramping;
                    trigger = true;
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


    template <typename enum_t>
    class EnumSmoother : public Smoother
    {
    private:
        const enum_t initialValue;
        const char *jsonKey;

    public:
        enum_t currentValue;
        enum_t targetValue;

        explicit EnumSmoother(enum_t init, const char *key, double _rampSeconds = DefaultRampSeconds)
            : Smoother(_rampSeconds)
            , initialValue(init)
            , jsonKey(key)
        {
            EnumSmoother_initialize();
        }

        void initialize() override
        {
            Smoother::initialize();
            EnumSmoother_initialize();
        }

        void EnumSmoother_initialize()
        {
            currentValue = initialValue;
            targetValue  = initialValue;
        }

        bool changed() const override
        {
            return currentValue != targetValue;
        }

        void onSilent() override
        {
            currentValue = targetValue;
        }

        void jsonSave(json_t* root)
        {
            jsonSetEnum(root, jsonKey, currentValue);
        }

        void jsonLoad(json_t* root)
        {
            jsonLoadEnum(root, jsonKey, currentValue);
            targetValue = currentValue;
        }

        void beginBumpEnum(int increment = +1)
        {
            targetValue = NextEnumValue(currentValue, increment);
        }
    };
}
