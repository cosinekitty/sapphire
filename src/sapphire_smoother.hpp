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

        virtual void initialize()
        {
            Smoother_initialize();
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

        double process(double sampleRateHz)
        {
            if (state == State::Stable)
            {
                if (!changed())
                    return gain;

                state = State::Fading;
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
                    state = State::Stable;
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


    template <typename enum_t>
    struct BumpEnumAction : history::Action
    {
        EnumSmoother<enum_t>& smoother;
        const int direction;

        explicit BumpEnumAction(EnumSmoother<enum_t>& _smoother, int _direction = +1)
            : smoother(_smoother)
            , direction(_direction)
            {}

        void undo() override
        {
            smoother.beginBumpEnum(-direction);
        }

        void redo() override
        {
            smoother.beginBumpEnum(+direction);
        }
    };
}
