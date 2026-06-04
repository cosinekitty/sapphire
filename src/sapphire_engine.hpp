#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <type_traits>

#include "sapphire_simd.hpp"

namespace Sapphire
{
    constexpr int ChaosOctaveRange = 7;     // the number of octaves above *OR* below zero for chaos SPEED knobs.

    constexpr double C4_FREQUENCY_HZ = 261.6255653005986;    // note C4 = (440 / (2**0.75)) Hz, because C4 is 3/4 octave below A4.

    template <typename value_t>
    inline value_t ClampInt(value_t x, int lo, int hi)
    {
        return std::clamp<value_t>(x, lo, hi);
    }

    template <typename value_t>
    constexpr value_t Square(value_t x)
    {
        return x * x;
    }

    template <typename value_t>
    constexpr value_t Cube(value_t x)
    {
        return x * x * x;
    }

    template <typename value_t>
    constexpr value_t FourthPower(value_t x)
    {
        value_t s = x * x;
        return s * s;
    }

    template <typename value_t>
    inline value_t TwoToPower(value_t x)
    {
        return std::exp2(x);
    }

    template <typename value_t>
    inline value_t OneHalfToPower(value_t x)
    {
        return TwoToPower(-x);
    }

    template <typename value_t>
    inline value_t TenToPower(value_t x)
    {
        static constexpr value_t L = 2.302585092994046;    // ln(10)
        return std::exp(static_cast<value_t>(L*x));
    }

    template <typename value_t>
    constexpr value_t CubicMix(value_t mix, value_t dry, value_t wet)
    {
        const value_t k = Cube<value_t>(1-mix);
        return k*dry + (1-k)*wet;
    }

    template <typename value_t>
    constexpr value_t LinearMix(value_t mix, value_t dry, value_t wet)
    {
        return (1-mix)*dry + mix*wet;
    }

    template <typename real_t>
    constexpr real_t BicubicLimiter(real_t x, real_t yLimit)
    {
        real_t xLimit = (3*yLimit)/2;

        if (x <= -xLimit)
            return -yLimit;

        if (x >= +xLimit)
            return +yLimit;

        return x - (4*x*x*x)/(27*yLimit*yLimit);
    }

    inline int MOD(int i, int n)    // Always returns 0..(n-1), even when i<0.
    {
        if (n <= 0)
            throw std::out_of_range(std::string("Invalid denominator for MOD: ") + std::to_string(n));

        const int m = ((i%n) + n) % n;
        if (m < 0 || m >= n)
            throw std::range_error("MOD internal failure.");

        return m;
    }

    template <typename real_t>
    inline real_t FMOD(real_t x, real_t y)
    {
        if (y <= 0)
            throw std::out_of_range(std::string("Invalid denominator for FMOD: ") + std::to_string(y));

        const real_t m = std::fmod(y + std::fmod(x, y), y);
        if (m < 0 || m >= y)
            throw std::range_error("FMOD internal failure.");

        return m;
    }

    template <typename enum_t>
    inline enum_t NextEnumValue(enum_t e, int increment = +1)
    {
        static constexpr int length = static_cast<int>(enum_t::LEN);
        static_assert(length > 0);
        const int value = static_cast<int>(e);
        const int next = MOD(value + increment, length);
        return static_cast<enum_t>(next);
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

            float gain = std::clamp<float>(static_cast<float>(count) / static_cast<float>(rampLength), 0, 1);

            for (int c = 0; c < channels; ++c)
                volts[c] *= gain;
        }
    };

    template <typename value_t>
    class LoHiPassFilter
    {
    private:
        value_t xprev {};
        value_t yprev {};
        float fc = 20;

    public:
        void Snap(value_t xDcLevel)
        {
            // Put the filter into the exact state of having been
            // fed a constant value `xDcLevel` for an infinite amount of time.
            // This causes the filter to instantly "settle" at xDcLevel.
            yprev = xprev = xDcLevel;
        }

        void Reset() { Snap(0); }
        void SetCutoffFrequency(float cutoffFrequencyHz) { fc = cutoffFrequencyHz; }

        void Update(value_t x, float sampleRateHz)
        {
            float c = sampleRateHz / (M_PI * fc);
            yprev = (x + xprev - yprev*(1 - c)) / (1 + c);
            xprev = x;
        }

        value_t HiPass() const { return xprev - yprev; }
        value_t LoPass() const { return yprev; }
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

        // The "Snap[Lo|Hi]Pass" functions allow restarting a simulation that was
        // already in progress, without encountering a popping artifact from the sudden
        // change in particle positions.

        value_t SnapLoPass(value_t x)
        {
            value_t y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Snap(y);
                y = stage[i].LoPass();
            }
            return y;
        }

        value_t SnapHiPass(value_t x)
        {
            value_t y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Snap(y);
                y = stage[i].HiPass();
            }
            return y;
        }
    };


    enum class FilterMode
    {
        Lowpass,
        Bandpass,
        Highpass,
        LEN,

        Default = Bandpass
    };


    enum class InterpolatorKind
    {
        Linear,
        Sinc,
        LEN,

        Default = Linear
    };


    inline float ExtremeValue(unsigned length, const float data[], float carry)
    {
        using namespace std;

        float extreme = carry;
        for (unsigned i = 0; i < length; ++i)
            extreme = max(extreme, abs(data[i]));
        return extreme;
    }



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
        float prevmax = 0.0;
        float currmax = 0.0;

    public:
        void update(double sampleRate, float extreme)
        {
            using namespace std;

            // Prevent pollution of AGC internal state.
            // Ignore non-finite numbers (pretend they are zero).
            if (!std::isfinite(extreme))
                extreme = 0;

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
                currmax = extreme;
            }
            else
            {
                --countdown;
                currmax = max(currmax, extreme);
            }

            double ratio = max(prevmax, currmax) / ceiling;
            double factor = (ratio >= follower) ? attackFactor : decayFactor;
            follower = max<double>(1, follower*factor + ratio*(1-factor));
        }

        static double VerifyPositive(double x)
        {
            if (!std::isfinite(x))
                throw std::range_error("AGC coefficient is NAN/INF.");
            if (x <= 0.0)
                throw std::range_error("AGC coefficient must be positive.");
            return x;
        }

        void initialize()
        {
            follower = 1.0;
            prevmax = currmax = 0.0;
        }

        double getFollower() const
        {
            return follower;
        }

        float getCeiling() const
        {
            return ceiling;
        }

        void setCeiling(float _ceiling)
        {
            ceiling = VerifyPositive(_ceiling);
        }

        void attenuate(unsigned length, float data[])
        {
            for (unsigned i = 0; i < length; ++i)
                data[i] /= follower;
        }

        void process(double sampleRate, unsigned nchannels, float frame[])
        {
            float input = ExtremeValue(nchannels, frame, 0);
            update(sampleRate, input);
            attenuate(nchannels, frame);
        }

        void process(double sampleRate, float& left, float& right)      // used for modules with stereo output
        {
            using namespace std;

            float input = max(abs(left), abs(right));
            update(sampleRate, input);
            left  /= follower;
            right /= follower;
        }

        void process(double sampleRate, std::vector<float>& buffer)     // used for modules with any number of outputs
        {
            process(sampleRate, buffer.size(), buffer.data());
        }
    };


    template <typename item_t, std::size_t bufsize = 10000>
    class DelayLine
    {
    private:
        static_assert(bufsize > 1, "The buffer must have room for more than 1 sample.");

        std::vector<item_t> buffer;
        std::size_t front = 1;               // postion where data is inserted
        std::size_t back = 0;                // postion where data is removed

    public:
        DelayLine()
        {
            buffer.resize(bufsize);
        }

        item_t readForward(std::size_t offset) const
        {
            // Access an item at an integer offset toward the future from the back of the delay line.
            if (offset >= bufsize)
                throw std::range_error("Delay line offset is out of bounds.");
            return buffer.at((back + offset) % bufsize);
        }

        item_t readBackward(std::size_t offset) const
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

        std::size_t getMaxLength() const
        {
            return bufsize - 1;
        }

        std::size_t getLength() const
        {
            return ((bufsize + front) - back) % bufsize;
        }

        std::size_t setLength(std::size_t requestedSamples)
        {
            // If the requested number of samples is invalid, clamp it to the valid range.
            // Essentially, we do the best we can, but exact pitch control is only possible
            // within certain bounds.
            std::size_t nsamples = std::clamp<std::size_t>(requestedSamples, 1, getMaxLength());

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


    inline float SlowTaper(float x, std::size_t steps)
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
        const std::size_t steps;
        const std::size_t nsegments;
        std::vector<float> table;

    public:
        InterpolatorTable(std::size_t _steps, std::size_t _nsegments)
            : steps(_steps)
            , nsegments(_nsegments | 1)     // IMPORTANT: force `nsegments` to be an odd integer!
        {
            // Pre-calculate an interpolation table over the range x = [0, steps+1].
            table.resize(nsegments);
            for (std::size_t i = 0; i < nsegments; ++i)
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
            std::size_t imid = static_cast<std::size_t>(std::round(ir));

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


    template <typename item_t, std::size_t steps>
    class Interpolator
    {
    private:
        static const InterpolatorTable table;
        static const std::size_t nsamples = 1 + 2*steps;
        item_t buffer[nsamples] {};

    public:
        void write(int position, item_t value)
        {
            std::size_t index = static_cast<std::size_t>(static_cast<int>(steps) + position);
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

    template <typename item_t, std::size_t steps>
    const InterpolatorTable Interpolator<item_t, steps>::table {steps, 0x801};

    //-----------------------------------------------------------------------------------------


    template <typename value_t>
    struct FilterResult
    {
        value_t lowpass{};
        value_t bandpass{};
        value_t highpass{};
        value_t notch{};

        explicit FilterResult() {}

        explicit FilterResult(const value_t& dry)
            : lowpass(dry)
            , bandpass(dry)
            , highpass(dry)
            , notch(dry)
        {
        }

        explicit FilterResult(const value_t& lp, const value_t& bp, const value_t& hp, const value_t& nx)
            : lowpass(lp)
            , bandpass(bp)
            , highpass(hp)
            , notch(nx)
        {
        }

        value_t select(FilterMode mode) const
        {
            switch (mode)
            {
            case FilterMode::Lowpass:   return lowpass;
            case FilterMode::Bandpass:  return bandpass;
            case FilterMode::Highpass:  return highpass;
            default:                    return 0;
            }
        }

        static FilterResult<value_t> Interpolate(
            value_t fraction,
            const FilterResult<value_t>& a,
            const FilterResult<value_t>& b)
        {
            return FilterResult<value_t>(
                LinearMix<value_t>(fraction, a.lowpass,  b.lowpass),
                LinearMix<value_t>(fraction, a.bandpass, b.bandpass),
                LinearMix<value_t>(fraction, a.highpass, b.highpass),
                LinearMix<value_t>(fraction, a.notch,    b.notch)
            );
        }
    };


    template <typename value_t>
    class StateVariableFilter
    {
    private:
        value_t c1{};
        value_t c2{};
        value_t bandpass{};
        value_t lowpass{};
        value_t v3{};
        value_t a1{};
        value_t a2{};
        value_t a3{};

        float prevFreqRatio{};
        float prevResonance{};
        float k{};

    public:
        void initialize()
        {
            c1 = 0;
            c2 = 0;
            bandpass = 0;
            lowpass = 0;
            v3 = 0;
        }

        FilterResult<value_t> process(float sampleRateHz, float cornerFreqHz, float resonance, const value_t& input)
        {
            // Based on: https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

            const float ratio = cornerFreqHz / sampleRateHz;
            if (ratio != prevFreqRatio || resonance != prevResonance)
            {
                prevFreqRatio = ratio;
                prevResonance = resonance;

                float g = std::tan(M_PI * ratio);
                const float cushion = 0.002;    // values of `k` too close to zero cause excessive ringing and aliasing
                k = cushion + ((2-cushion) * Cube(1-resonance));
                a1 = 1 / (1 + g*(g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            v3 = input - c2;
            bandpass = a1*c1 + a2*v3;
            lowpass = c2 + a2*c1 + a3*v3;
            c1 = 2*bandpass - c1;
            c2 = 2*lowpass - c2;

            const value_t notch = input - k*bandpass;
            return FilterResult<value_t>(lowpass, bandpass, notch - lowpass, notch);
        }
    };

    //-----------------------------------------------------------------------------------------

    constexpr unsigned NEED_LP  = 1;
    constexpr unsigned NEED_BP  = 2;
    constexpr unsigned NEED_HP  = 4;
    constexpr unsigned NEED_NX  = 8;
    constexpr unsigned NEED_ALL = NEED_LP | NEED_BP | NEED_HP | NEED_NX;

    template <typename value_t, unsigned MAX_FILTER_STAGES>
    class CascadeStateVariableFilter
    {
        static_assert(MAX_FILTER_STAGES >= 2);
        static constexpr unsigned REMAINING_STAGES     = MAX_FILTER_STAGES - 1;
        static constexpr unsigned STAGES_INCLUDING_DRY = MAX_FILTER_STAGES + 1;
        using filter_t = StateVariableFilter<value_t>;

    private:
        value_t cascade = 1;
        filter_t firstStage;
        std::array<filter_t, REMAINING_STAGES> lpStage;
        std::array<filter_t, REMAINING_STAGES> bpStage;
        std::array<filter_t, REMAINING_STAGES> hpStage;
        std::array<filter_t, REMAINING_STAGES> nxStage;

    public:
        unsigned mask = NEED_ALL;

        void initialize()
        {
            cascade = 1;
            firstStage.initialize();
            for (filter_t& f : lpStage)  f.initialize();
            for (filter_t& f : bpStage)  f.initialize();
            for (filter_t& f : hpStage)  f.initialize();
            for (filter_t& f : nxStage)  f.initialize();
            mask = NEED_ALL;
        }

        void setCascade(const value_t& newCascade)
        {
            cascade = std::clamp<value_t>(newCascade, 1, MAX_FILTER_STAGES);
        }

        using result_t = FilterResult<value_t>;

        result_t process(
            float sampleRateHz,
            value_t cornerFreqHz,
            value_t resonance,
            const value_t& input)
        {
            unsigned k = static_cast<unsigned>(std::floor(cascade));
            float fraction = cascade - k;
            if (k >= MAX_FILTER_STAGES)
            {
                assert(k == MAX_FILTER_STAGES);
                k = MAX_FILTER_STAGES - 1;
                fraction = 1;
            }

            std::array<result_t, STAGES_INCLUDING_DRY> iter;

            iter[0] = result_t(input);      // start with dry input as "0 iterations" for LP, BP, HP, N.

            // The first filter stage can be handled with a single call
            // to get (lowpass, bandpass, highpass, notch) all in parallel.

            iter[1] = firstStage.process(sampleRateHz, cornerFreqHz, resonance, input);
            if (cascade == 1)
                return iter[1];     // optimization to avoid needless calculation for classic-style Sauce behavior.

            // When cascade is above 1, we need extra filter calls, 4 per stage,
            // because we bandpass(bandpass(bandpass(...))),
            // notch(notch(notch(...))),
            // and so on.

            if (mask & NEED_LP)
            {
                unsigned s = 2;
                for (filter_t& f : lpStage)
                {
                    if (s > k+1)
                        break;  // iter[k+1] is the highest filter index we need calculated
                    iter.at(s).lowpass = f.process(sampleRateHz, cornerFreqHz, resonance, iter.at(s-1).lowpass).lowpass;
                    ++s;
                }
            }

            if (mask & NEED_BP)
            {
                unsigned s = 2;
                for (filter_t& f : bpStage)
                {
                    if (s > k+1)
                        break;  // iter[k+1] is the highest filter index we need calculated
                    iter.at(s).bandpass = f.process(sampleRateHz, cornerFreqHz, resonance, iter.at(s-1).bandpass).bandpass;
                    ++s;
                }
            }

            if (mask & NEED_HP)
            {
                unsigned s = 2;
                for (filter_t& f : hpStage)
                {
                    if (s > k+1)
                        break;  // iter[k+1] is the highest filter index we need calculated
                    iter.at(s).highpass = f.process(sampleRateHz, cornerFreqHz, resonance, iter.at(s-1).highpass).highpass;
                    ++s;
                }
            }

            if (mask & NEED_NX)
            {
                unsigned s = 2;
                for (filter_t& f : nxStage)
                {
                    if (s > k+1)
                        break;  // iter[k+1] is the highest filter index we need calculated
                    iter.at(s).notch = f.process(sampleRateHz, cornerFreqHz, resonance, iter.at(s-1).notch).notch;
                    ++s;
                }
            }

            return result_t::Interpolate(fraction, iter.at(k), iter.at(k+1));
        }
    };

    //-----------------------------------------------------------------------------------------

    struct PanningFactors
    {
        float left{};
        float right{};

        explicit PanningFactors(float _left, float _right)
            : left(_left)
            , right(_right)
            {}
    };


    inline PanningFactors Panning(float knob)
    {
        float x = std::clamp<float>(knob, -1, +1);

        // Use a power law to smoothly transition from either channel dominating,
        // but with consistent power sum across both channels.
        const float theta = M_PI_4 * (x+1);       // map [-1, +1] onto [0, pi/2]
        const float leftFactor  = M_SQRT2 * std::cos(theta);
        const float rightFactor = M_SQRT2 * std::sin(theta);

        return PanningFactors(leftFactor, rightFactor);
    }

    //-----------------------------------------------------------------------------------------

    template <typename index_t, typename length_t>
    bool IsValidIndex(index_t index, length_t length)
    {
        static_assert(std::is_integral_v<index_t>);
        using u = std::make_unsigned_t<index_t>;

        static_assert(std::is_integral_v<length_t>);
        static_assert(std::is_unsigned_v<length_t>);

        return static_cast<u>(index) < length;
    }

    template <typename container_t, typename index_t>
    bool IsSafeAccess(const container_t& container, index_t index)
    {
        return IsValidIndex(index, container.size());
    }
}
