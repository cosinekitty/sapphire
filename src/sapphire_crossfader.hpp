#pragma once
#include <functional>

namespace Sapphire
{
    constexpr double DefaultCrossfadeDurationSeconds = 0.01;

    class Crossfader
    {
    private:
        double mix = 0;
        double target = 0;
        double duration = DefaultCrossfadeDurationSeconds;

    public:
        double setCrossfadeDuration(double durationSeconds)
        {
            duration = std::clamp<double>(durationSeconds, 0.001, 10.0);
            return duration;
        }

        void snapToFront()
        {
            target = mix = 0;
        }

        void snapToBack()
        {
            target = mix = 1;
        }

        bool atFront() const
        {
            return mix <= 0;
        }

        bool atBack() const
        {
            return mix >= 1;
        }

        bool inTransition() const
        {
            return !atFront() && !atBack();
        }

        bool isFrontTargeted() const
        {
            return target < 0.5;
        }

        bool isBackTargeted() const
        {
            return target > 0.5;
        }

        void beginFadeToFront()
        {
            target = 0;
        }

        void beginFadeToBack()
        {
            target = 1;
        }

        void beginFade(bool back)
        {
            target = back ? 1 : 0;
        }

        float process(
            double sampleRateHz,
            std::function<float()> getFrontValue,
            std::function<float()> getBackValue)
        {
            if (atFront() && isFrontTargeted())
                return getFrontValue();

            if (atBack() && isBackTargeted())
                return getBackValue();

            float front = getFrontValue();
            float back  = getBackValue();
            float value = (1-mix)*front + mix*back;

            double incr = 1.0 / (duration * sampleRateHz);
            if (mix < target)
                mix = std::clamp<double>(mix + incr, 0, 1);
            else if (mix > target)
                mix = std::clamp<double>(mix - incr, 0, 1);

            return value;
        }

        float process(double sampleRateHz, float frontValue, float backValue)
        {
            return process(
                sampleRateHz,
                [=]{ return frontValue; },
                [=]{ return backValue; }
            );
        }
    };
}
