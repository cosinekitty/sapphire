#ifndef __COSINEKITTY_SAPPHIRE_ENGINE_HPP
#define __COSINEKITTY_SAPPHIRE_ENGINE_HPP

#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>
#include <stdexcept>

#ifdef NO_RACK_DEPENDENCY
/*
 * In the rack context this ifdef isn't needed; rack gives you simde for free
 * but if you want to use this module outside of rack (which we do) you need to
 * have a bring-your-own simde approach which will just use RACK SIMDE in rack builds
 */
#if defined(__arm64)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse4.2.h"
#else
#include <pmmintrin.h>
#endif

#else
#include "rack.hpp"
#endif

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

    inline size_t Clamp(size_t x, size_t minValue, size_t maxValue)
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

        SlewState state = Disabled;
        int rampLength = 1;
        int count = 0;          // IMPORTANT: valid only when state == Ramping; must ignore otherwise

    public:
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

            if (rampLength <= 0)
                return;     // prevent division by zero or other weird behavior

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

        PhysicsVector(float x)      // cppcheck-suppress [noExplicitConstructor]
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

    template <typename value_t>
    class LoHiPassFilter
    {
    private:
        bool  first {true};
        value_t xprev {};
        value_t yprev {};
        float fc {20.0f};

    public:
        void Reset() { first = true; }
        void SetCutoffFrequency(float cutoffFrequencyHz) { fc = cutoffFrequencyHz; }

        void Update(value_t x, float sampleRateHz)
        {
            if (first)
            {
                first = false;
                yprev = x;
            }
            else
            {
                float c = sampleRateHz / (M_PI * fc);
                yprev = (x + xprev - yprev*(1 - c)) / (1 + c);
            }
            xprev = x;
        }

        value_t HiPass() const { return xprev - yprev; }
        value_t LoPass() const { return yprev; };
    };


    template <typename value_t, int LAYERS>
    class StagedFilter
    {
    private:
        LoHiPassFilter<value_t> stage[LAYERS];

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

        value_t UpdateLoPass(value_t x, float sampleRateHz)
        {
            value_t y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Update(y, sampleRateHz);
                y = stage[i].LoPass();
            }
            return y;
        }

        value_t UpdateHiPass(value_t x, float sampleRateHz)
        {
            value_t y = x;
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

        SliderMapping(SliderScale _scale, const std::vector<float>& _polynomial)
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


    class AutomaticGainLimiter      // dynamically adjusts gain to keep a signal from getting too hot
    {
    private:
        double ceiling = 1.0;
        double attackHalfLife = 0.005;
        double decayHalfLife = 0.1;
        double attackFactor = 0.0;
        double decayFactor = 0.0;
        double follower = 1.0;
        double cachedSampleRate = 0.0;
        const int PERIODS_PER_SECOND = 4;
        int countdown = 0;
        float prevmax = 0.0f;
        float currmax = 0.0f;

        void update(double sampleRate, float input)
        {
            using namespace std;

            if (sampleRate != cachedSampleRate)
            {
                cachedSampleRate = sampleRate;
                attackFactor = pow(0.5, 1.0 / (sampleRate * attackHalfLife));
                decayFactor  = pow(0.5, 1.0 / (sampleRate * decayHalfLife));
            }

            if (countdown <= 0)
            {
                countdown = static_cast<int>(round(sampleRate / PERIODS_PER_SECOND));
                prevmax = currmax;
                currmax = input;
            }
            else
            {
                --countdown;
                currmax = max(currmax, input);
            }

            double ratio = max(prevmax, currmax) / ceiling;
            double factor = (ratio >= follower) ? attackFactor : decayFactor;
            follower = max(1.0, follower*factor + ratio*(1-factor));
        }

        static double VerifyPositive(double x)
        {
            if (x <= 0.0)
                throw std::range_error("AGC coefficient must be positive.");
            return x;
        }

    public:
        void initialize()
        {
            follower = 1.0;
            prevmax = currmax = 0.0f;
        }

        void setCeiling(float _ceiling)
        {
            ceiling = VerifyPositive(_ceiling);
        }

        void process(double sampleRate, float& left, float& right)
        {
            using namespace std;

            float input = max(abs(left), abs(right));
            update(sampleRate, input);
            left  /= follower;
            right /= follower;
        }

        double getFollower() const
        {
            return follower;
        }
    };


    template <typename item_t, size_t bufsize = 10000>
    class DelayLine
    {
    private:
        static_assert(bufsize > 1, "The buffer must have room for more than 1 sample.");

        std::vector<item_t> buffer;
        size_t front = 1;               // postion where data is inserted
        size_t back = 0;                // postion where data is removed

    public:
        DelayLine()
        {
            buffer.resize(bufsize);
        }

        item_t readForward(size_t offset) const
        {
            // Access an item at an integer offset toward the future from the back of the delay line.
            if (offset >= bufsize)
                throw std::range_error("Delay line offset is out of bounds.");
            return buffer.at((back + offset) % bufsize);
        }

        item_t readBackward(size_t offset) const
        {
            // Access an item at an integer offset into the past from the front of the delay line.
            if (offset >= bufsize)
                throw std::range_error("Delay line offset is out of bounds.");
            return buffer.at(((bufsize + front) - (offset + 1)) % bufsize);
        }

        void write(const item_t& x)
        {
            buffer.at(front) = x;
            front = (front + 1) % bufsize;
            back = (back + 1) % bufsize;
        }

        size_t getMaxLength() const
        {
            return bufsize - 1;
        }

        size_t getLength() const
        {
            return ((bufsize + front) - back) % bufsize;
        }

        size_t setLength(size_t requestedSamples)
        {
            // If the requested number of samples is invalid, clamp it to the valid range.
            // Essentially, we do the best we can, but exact pitch control is only possible
            // within certain bounds.
            size_t nsamples = Clamp(requestedSamples, static_cast<size_t>(1), getMaxLength());

            // Leave `front` where it is. Adjust `back` forward or backward as needed.
            // If `front` and `back` are the same, then the length is 1 sample,
            // because the usage contract is to to call read() before calling write().
            back = ((front + bufsize) - nsamples) % bufsize;

            assert(nsamples == getLength());

            // Let the caller know how many samples we actually used for the delay line length.
            return nsamples;
        }

        void clear()
        {
            for (item_t& x : buffer)
                x = {};
        }
    };

    inline float Sinc(float x)
    {
        float angle = std::abs(static_cast<float>(M_PI * x));
        if (angle < 1.0e-6f)
            return 1.0f;
        return std::sin(angle) / angle;
    }


    inline float Blackman(float x)
    {
        // https://www.mathworks.com/help/signal/ref/blackman.html
        // Blackman(0.0) = 0.0
        // Blackman(0.5) = 1.0
        // Blackman(1.0) = 0.0
        return 0.42f - 0.5f*std::cos(static_cast<float>(2*M_PI) * x) + 0.08f*std::cos(static_cast<float>(4*M_PI) * x);
    }


    inline float SlowTaper(float x, size_t steps)
    {
        float sinc = Sinc(x);
        float taper = Blackman((x + (steps+1)) / (2*(steps+1)));
        return sinc * taper;
    }


    inline float QuadInterp(float Q, float R, float S, float x)
    {
        return (((S + Q)/2 - R)*x + ((S - Q)/2))*x + R;
    }


    class InterpolatorTable
    {
    private:
        const size_t steps;
        const size_t nsegments;
        std::vector<float> table;

    public:
        InterpolatorTable(size_t _steps, size_t _nsegments)
            : steps(_steps)
            , nsegments(_nsegments | 1)     // IMPORTANT: force `nsegments` to be an odd integer!
        {
            // Pre-calculate an interpolation table over the range x = [0, steps+1].
            table.resize(nsegments);
            for (size_t i = 0; i < nsegments; ++i)
            {
                float x = static_cast<float>(i * (steps+1)) / static_cast<float>(nsegments-1);
                table[i] = SlowTaper(x, steps);
            }
        }

        float Taper(float x) const
        {
            // The taper function is even, so we can cut the range in half with absolute value:
            x = std::abs(x);

            // Calculate the real-valued table index.
            float ir = (x * (nsegments-1)) / (steps+1);

            // The taper window goes to zero at the endpoints,
            // and we consider it uniformly zero outside the window of relevance.
            if (ir > nsegments-1)
                return 0.0f;

            // We will find a best-fit parabola passing through 3 points.
            // Do not allow any discontinuities in the curve!
            // That means we cannot pick two different parabolas for
            // nearby real values `ir` except across integer values where
            // both parabolas will converge to the same endpoint.
            // We already forced `nsegments` to be an odd number, so that we can
            // always break the regions into a whole number of parabolas.
            // Example: suppose nsegments = 7. Then the following index groups
            // are used to calculate parabolas:
            //
            //      [0, 1, 2]: for 0 <= ir <= 2
            //      [2, 3, 4]: for 2 <  ir <= 4
            //      [4, 5, 6]: for 4 <  ir <= 6
            //
            // The general rule is: the leftmost of the 3 indices must be even.
            // All 3 indices must be in the range [0, nsegments-1].

            // Round to the nearest integer for the central index.
            size_t imid = static_cast<size_t>(std::round(ir));

            // `di` = Fractional distance from the central index.
            float di = ir - static_cast<float>(imid);

            if (imid == 0)
            {
                // We must shift imid upward by one slot.
                ++imid;
                di -= 1.0f;
            }
            else if (imid == nsegments-1)
            {
                // We must shift imid downward by one slot.
                --imid;
                di += 1.0f;
            }
            else if (0 == (imid & 1))
            {
                // The middle index is not allowed to be even like this.
                // Adjust it up or down as needed.
                if (di < 0.0f)
                {
                    --imid;
                    di += 1.0f;
                }
                else
                {
                    ++imid;
                    di -= 1.0f;
                }
            }

            assert(imid & 1);
            return QuadInterp(table.at(imid-1), table.at(imid), table.at(imid+1), di);
        }
    };


    template <typename item_t, size_t steps>
    class Interpolator
    {
    private:
        static const InterpolatorTable table;
        static const size_t nsamples = 1 + 2*steps;
        item_t buffer[nsamples] {};

    public:
        void write(int position, item_t value)
        {
            size_t index = static_cast<size_t>(static_cast<int>(steps) + position);
            if (index >= nsamples)
                throw std::range_error("Interpolator write position is out of bounds.");
            buffer[index] = value;
        }

        item_t read(float position) const
        {
            if (position < -1.0f || position > +1.0f)
                throw std::range_error("Interpolator read position is out of bounds.");

            const int s = static_cast<int>(steps);
            item_t sum {};
            for (int n = -s; n <= s; ++n)
                sum += buffer[n+s] * table.Taper(position - n);

            return sum;
        }
    };

    template <typename item_t, size_t steps>
    const InterpolatorTable Interpolator<item_t, steps>::table {steps, 0x801};
}

#endif  // __COSINEKITTY_SAPPHIRE_ENGINE_HPP
