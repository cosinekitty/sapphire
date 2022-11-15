#ifndef __COSINEKITTY_SAPPHIRE_ENGINE_HPP
#define __COSINEKITTY_SAPPHIRE_ENGINE_HPP

#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>
#include <pmmintrin.h>

namespace Sapphire
{
    inline float Clamp(float x, float minValue = 0.0f, float maxValue = 1.0f)
    {
        if (x < minValue)
            return minValue;

        if (x > maxValue)
            return maxValue;

        return x;
    }

    class Slewer
    {
    private:
        enum SlewState
        {
            Disabled,       // there is no audio slewing: treat inputs as control voltages
            Off,            // audio slewing is enabled, but currently the output is disconnected
            Ramping,        // either a rising or falling linear ramp transitioning between connect/disconnect
            On,             // audio slewing is enabled, and currently the output is connected
        };

        SlewState state;
        int rampLength;
        int count;          // IMPORTANT: valid only when state == Ramping; must ignore otherwise

    public:
        Slewer()
            : state(Disabled)
            , rampLength(1)
            , count(0)
            {}

        void setRampLength(int newRampLength)
        {
            rampLength = std::max(1, newRampLength);
        }

        void reset()
        {
            // Leave the rampLength alone. It should only be changed by changes in the sample rate.
            state = Disabled;
        }

        void enable(bool active)
        {
            state = active ? On : Off;
        }

        bool isEnabled() const
        {
            return state != Disabled;
        }

        bool update(bool active)
        {
            switch (state)
            {
            case Disabled:
                return active;

            case Off:
                if (active)
                {
                    // Start an upward ramp.
                    state = Ramping;
                    count = 0;
                }
                break;

            case On:
                if (!active)
                {
                    // Start a downward ramp.
                    state = Ramping;
                    count = rampLength - 1;
                }
                break;

            case Ramping:
                // Allow zig-zagging of the linear ramp if `active` keeps changing.
                if (active)
                {
                    // Ramp upward.
                    if (count < rampLength)
                        ++count;
                    else
                        state = On;
                }
                else
                {
                    // Ramp downward.
                    if (count > 0)
                        --count;
                    else
                        state = Off;
                }
                break;

            default:
                assert(false);      // invalid state -- should never happen
                break;
            }

            return state != Off;
        }

        void process(float volts[], int channels)
        {
            if (state != Ramping)
                return;     // not ramping, so we must ignore `count`

            if (channels < 1)
                return;     // another short-cut to save processing time

            // The sample rate could change at any moment,
            // including while we are ramping.
            // Therefore we need to make sure the ratio count/rampLength
            // is bounded to the range [0, 1].

            float gain = Clamp(static_cast<float>(count) / static_cast<float>(rampLength));

            for (int c = 0; c < channels; ++c)
                volts[c] *= gain;
        }
    };

    class PhysicsVector
    {
    public:
        union {
            __m128 v;
            float s[4];
        };

        PhysicsVector()
            : v(_mm_set1_ps(0.0f))
            {}

        PhysicsVector(float x)
            : v(_mm_set1_ps(x))
            {}

        explicit PhysicsVector(__m128 _v)
            : v(_v)
            {}

        PhysicsVector(float x1, float x2, float x3, float x4)
            : v(_mm_setr_ps(x1, x2, x3, x4))
            {}

        float& operator[](int i) { return s[i]; }
        const float& operator[](int i) const { return s[i]; }
        static PhysicsVector zero() { return PhysicsVector(0.0); }
    };

    inline PhysicsVector operator + (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_add_ps(a.v, b.v));
    }

    inline PhysicsVector operator - (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_sub_ps(a.v, b.v));
    }

    inline PhysicsVector operator * (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_mul_ps(a.v, b.v));
    }

    inline PhysicsVector& operator += (PhysicsVector& a, const PhysicsVector& b)
    {
        return a = a + b;
    }

    inline PhysicsVector& operator -= (PhysicsVector& a, const PhysicsVector& b)
    {
        return a = a - b;
    }

    inline PhysicsVector& operator *= (PhysicsVector& a, const PhysicsVector& b)
    {
        return a = a * b;
    }

    inline float Dot(const PhysicsVector &a, const PhysicsVector &b)
    {
        PhysicsVector c = a * b;
        return c.s[0] + c.s[1] + c.s[2] + c.s[3];
    }

    inline PhysicsVector Cross(const PhysicsVector &a, const PhysicsVector &b)
    {
        return PhysicsVector(
            a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0],
            0.0f
        );
    }

    inline float Magnitude(const PhysicsVector &a)
    {
        return std::sqrt(Dot(a, a));
    }

    inline PhysicsVector Interpolate(
        float slider,
        const PhysicsVector &a,
        const PhysicsVector &b)
    {
        PhysicsVector c = (1.0-slider)*a + slider*b;

        // Normalize the length to the match the first vector `a`.
        return sqrt(Dot(a,a) / Dot(c,c)) * c;
    }

    using PhysicsVectorList = std::vector<PhysicsVector>;

    class LoHiPassFilter
    {
    private:
        bool  first;
        float xprev;
        float yprev;
        float fc;

    public:
        LoHiPassFilter()
            : first(true)
            , xprev(0.0)
            , yprev(0.0)
            , fc(20.0)
            {}

        void Reset() { first = true; }
        void SetCutoffFrequency(float cutoffFrequencyHz) { fc = cutoffFrequencyHz; }
        void Update(float x, float sampleRateHz);
        float HiPass() const { return xprev - yprev; }
        float LoPass() const { return yprev; };
    };


    template <int LAYERS>
    class StagedFilter
    {
    private:
        LoHiPassFilter stage[LAYERS];

    public:
        void Reset()
        {
            for (int i = 0; i < LAYERS; ++i)
                stage[i].Reset();
        }

        void SetCutoffFrequency(float cutoffFrequencyHz)
        {
            for (int i = 0; i < LAYERS; ++i)
                stage[i].SetCutoffFrequency(cutoffFrequencyHz);
        }

        float UpdateLoPass(float x, float sampleRateHz)
        {
            float y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Update(y, sampleRateHz);
                y = stage[i].LoPass();
            }
            return y;
        }

        float UpdateHiPass(float x, float sampleRateHz)
        {
            float y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Update(y, sampleRateHz);
                y = stage[i].HiPass();
            }
            return y;
        }
    };


    enum class SliderScale
    {
        Linear,         // evaluate the polynomial and return the resulting value `y`
        Exponential,    // evaluate the polynomial as `y` and then return 10^y
    };


    class SliderMapping     // maps a slider value onto an arbitrary polynomial expression
    {
    private:
        SliderScale scale = SliderScale::Linear;
        std::vector<float> polynomial;     // polynomial coefficients, where index = exponent

    public:
        SliderMapping() {}

        SliderMapping(SliderScale _scale, std::vector<float> _polynomial)
            : scale(_scale)
            , polynomial(_polynomial)
            {}

        float Evaluate(float x) const
        {
            float y = 0.0;
            float xpower = 1.0;
            for (float coeff : polynomial)
            {
                y += coeff * xpower;
                xpower *= x;
            }

            switch (scale)
            {
            case SliderScale::Exponential:
                return pow(10.0, y);

            case SliderScale::Linear:
            default:
                return y;
            }
        }
    };
}

#endif  // __COSINEKITTY_SAPPHIRE_ENGINE_HPP
