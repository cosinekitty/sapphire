#include <cstdio>
#include <cstring>
#include <random>
#include "sapphire_engine.hpp"
#include "sapphire_random.hpp"
#include "sapphire_granular_processor.hpp"
#include "wavefile.hpp"
#include "chaos.hpp"

const char *MyVoiceFileName = "input/genesis.wav";


static int Fail(const std::string name, const std::string message)
{
    fprintf(stderr, "%s: FAIL - %s\n", name.c_str(), message.c_str());
    return 1;
}

static int Pass(const std::string name)
{
    printf("%s: PASS\n", name.c_str());
    return 0;
}

using UnitTestFunction = int (*) ();

struct UnitTest
{
    const char *name;
    UnitTestFunction func;
};

static int AutoGainControl();
static int GranuleTest();
static int ChaosTest();
static int ReadWave();
static int AutoScale();
static int DelayLineTest();
static int InterpolatorTest();
static int TaperTest();
static int QuadraticTest();

static const UnitTest CommandTable[] =
{
    { "agc",        AutoGainControl },
    { "granule",    GranuleTest },
    { "chaos",      ChaosTest },
    { "delay",      DelayLineTest },
    { "interp",     InterpolatorTest },
    { "quad",       QuadraticTest },
    { "readwave",   ReadWave },
    { "scale",      AutoScale },
    { "taper",      TaperTest },
    { nullptr,  nullptr }
};


int main(int argc, const char *argv[])
{
    if (argc == 2)
    {
        if (!strcmp(argv[1], "all"))
        {
            for (int i = 0; CommandTable[i].name; ++i)
                if (CommandTable[i].func())
                    return 1;
            return 0;
        }

        for (int i = 0; CommandTable[i].name; ++i)
            if (!strcmp(argv[1], CommandTable[i].name))
                return CommandTable[i].func();
    }

    fprintf(stderr, "unittest: Invalid command line parameters.\n");
    return 1;
}


class TestSignal
{
public:
    virtual void getSample(float& left, float& right) = 0;
};


class FilteredRandom
{
private:
    std::mt19937 rand;
    double rspan;
    double rmid;
    Sapphire::StagedFilter<float, 3> lo;
    Sapphire::StagedFilter<float, 3> hi;
    double amplitude;
    double sampleRate;

public:
    FilteredRandom(unsigned _seed, double _amplitude, double _sampleRate)
        : rand(_seed)
        , rspan((std::mt19937::max() - std::mt19937::min()) / 2.0)
        , rmid((std::mt19937::max() + std::mt19937::min()) / 2.0)
        , amplitude(_amplitude)
        , sampleRate(_sampleRate)
    {
        lo.SetCutoffFrequency(1000.0);      // low-pass everything below 1000 Hz
        hi.SetCutoffFrequency(20.0);        // high-pass everything above 20 Hz
    }

    float getSample()
    {
        float s = (rand() - rmid) / rspan;
        s = lo.UpdateLoPass(s, sampleRate);
        s = hi.UpdateHiPass(s, sampleRate);
        return s * amplitude * 2.53474809;        // experimental fudge factor: compensates for filter losses
    }
};


static int AgcTestCase(
    const char *name,
    TestSignal& signal,
    int sampleRate,
    int durationSeconds,
    double overshootTolerance)
{
    using namespace std;

    const double ceiling = 1.0;
    const double amplitude = 10.0;
    Sapphire::AutomaticGainLimiter agc;
    agc.setCeiling(ceiling);

    string harshFileName = string("output/agc_input_")  + name + ".wav";
    string mildFileName  = string("output/agc_output_") + name + ".wav";

    WaveFileWriter harsh;
    if (!harsh.Open(harshFileName.c_str(), sampleRate, 2))
    {
        printf("AgcTestCase(%s): Cannot open output wave file: %s\n", name, harshFileName.c_str());
        return 1;
    }

    WaveFileWriter mild;
    if (!mild.Open(mildFileName.c_str(), sampleRate, 2))
    {
        printf("AgcTestCase(%s): Cannot open output wave file: %s\n", name, mildFileName.c_str());
        return 1;
    }

    // Burn a few samples to get the filters to settle down.
    float left, right;
    for (int i = 0; i < 10000; ++i)
        signal.getSample(left, right);

    float maxHarsh = 0.0f;
    float maxMild = 0.0f;
    float sample[2];
    const int durationSamples = sampleRate * durationSeconds;
    for (int i = 0; i < durationSamples; ++i)
    {
        signal.getSample(left, right);

        maxHarsh = max(maxHarsh, max(abs(left), abs(right)));

        sample[0] = left / amplitude;
        sample[1] = right / amplitude;
        harsh.WriteSamples(sample, 2);

        agc.process(sampleRate, left, right);

        if (i > durationSamples / 4)
            maxMild = max(maxMild, max(abs(left), abs(right)));

        sample[0] = left / amplitude;
        sample[1] = right / amplitude;
        mild.WriteSamples(sample, 2);
    }

    printf("AgcTestCase(%s): maxHarsh = %0.6f, maxMild = %0.6f\n", name, maxHarsh, maxMild);

    const double ideal = amplitude / ceiling;
    const double overshoot = ideal / agc.getFollower();
    printf("AgcTestCase(%s): ideal follower = %0.6lf, actual follower = %0.6lf, overshoot = %0.6lf\n",
        name, ideal, agc.getFollower(), overshoot);

    int error = 0;

    if (maxHarsh < 9.99 || maxHarsh > 10.01)
    {
        printf("AgcTestCase(%s) FAIL: maxHarsh was out of bounds.\n", name);
        error = 1;
    }

    if (overshoot < 1.0 || overshoot > overshootTolerance)
    {
        printf("AgcTestCase(%s) FAIL: overshoot was out of bounds.\n", name);
        error = 1;
    }

    if (error == 0)
        printf("AgcTestCase(%s): PASS\n", name);

    return error;
}


class TestSignal_Random: public TestSignal
{
private:
    FilteredRandom lrandom;
    FilteredRandom rrandom;

public:
    TestSignal_Random(double amplitude, int sampleRate)
        : lrandom { 0x539a0c27, amplitude, static_cast<double>(sampleRate) }
        , rrandom { 0x7ac5b398, amplitude, static_cast<double>(sampleRate) }
        {}

    void getSample(float& left, float& right) override
    {
        left  = lrandom.getSample();
        right = rrandom.getSample();
    }
};


class TestSignal_Pulses: public TestSignal
{
private:
    double amplitude;
    int sampleRate;
    int pulseFreqHz;
    int countdown;

public:
    TestSignal_Pulses(double _amplitude, int _sampleRate, int _pulseFreqHz)
        : amplitude(_amplitude)
        , sampleRate(_sampleRate)
        , pulseFreqHz(_pulseFreqHz)
        , countdown(_sampleRate / _pulseFreqHz)
        {}

    void getSample(float& left, float& right) override
    {
        if (countdown == 0)
        {
            countdown = sampleRate / pulseFreqHz;
            left = right = amplitude;
        }
        else
        {
            --countdown;
            left = right = 0.0f;
        }
    }
};


static int AutoGainControl()
{
    const int sampleRate = 44100;
    const int durationSeconds = 5;
    const double amplitude = 10.0;

    {
        TestSignal_Random signal { amplitude, sampleRate };
        if (AgcTestCase("random", signal, sampleRate, durationSeconds, 1.084))
            return 1;
    }

    {
        TestSignal_Pulses signal { amplitude, sampleRate, 40 };
        if (AgcTestCase("pulses", signal, sampleRate, durationSeconds, 1.001))
            return 1;
    }

    return 0;
}


static int ReadWave()
{
    const char *inFileName = MyVoiceFileName;
    const char *outFileName = "output/genesis.wav";

    WaveFileReader inwave;
    if (!inwave.Open(inFileName))
        return Fail("ReadWave", std::string("Could not open input file: ") + inFileName);

    int sampleRate = inwave.SampleRate();
    int channels = inwave.Channels();
    if (sampleRate != 44100 || channels != 2)
    {
        fprintf(stderr, "ReadWave: FAIL - Expected 44100 Hz stereo, but found %d Hz, %d channels.\n", sampleRate, channels);
        return 1;
    }

    WaveFileWriter outwave;
    if (!outwave.Open(outFileName, sampleRate, channels))
        return Fail("ReadWave", std::string("Could not open output file: ") + outFileName);

    const size_t bufsize = 1024;
    std::vector<int16_t> buffer;
    buffer.resize(bufsize);

    for(;;)
    {
        size_t received = inwave.Read(buffer.data(), bufsize);
        outwave.WriteSamples(buffer.data(), received);
        if (received < bufsize)
            break;
    }

    return Pass("ReadWave");
}


static int AutoScale()
{
    // Verify that class ScaledWaveFileWriter can automatically adjust the level
    // of an output signal by a two-pass approach: write floating point data and
    // remember maximum amplitude, then go back and use maximum amplitude to scale
    // to 16-bit integer audio.

    const char *outFileName = "output/scale.wav";
    const int sampleRate = 44100;
    const int channels = 2;

    ScaledWaveFileWriter outwave;
    if (!outwave.Open(outFileName, sampleRate, channels))
        return Fail("AutoScale", std::string("Could not open output file: ") + outFileName);

    const size_t bufsize = 1024;
    const size_t durationBuffers = (3 * static_cast<size_t>(sampleRate * channels)) / bufsize;
    std::vector<float> buffer;
    buffer.resize(bufsize);

    float a = 1.0f;
    float b = 0.0f;
    float radians = (440.0 * 2.0 * M_PI) / sampleRate;
    float c = cos(radians);
    float s = sin(radians);

    for (size_t n = 0; n < durationBuffers; ++n)
    {
        for (size_t i = 0; i < bufsize; i += channels)
        {
            // Generate a stereo 440 Hz (cosine, sine) pair.
            buffer[i] = a;
            buffer[i+1] = b;
            float t = a*c - b*s;
            b = a*s + b*c;
            a = t;
        }

        outwave.WriteSamples(buffer.data(), bufsize);
    }

    return Pass("AutoScale");
}


static int DelayLineTest()
{
    using delay_t = Sapphire::DelayLine<float>;
    delay_t delay;
    const size_t m = delay.getMaxLength();

    // Should start out as a 1-sample buffer.
    if (delay.getLength() != 1)
        return Fail("DelayLineTest", std::string("Expected length=1, but found: ") + std::to_string(delay.getLength()));

    float x = delay.readForward(0);
    if (x != 0.0f)
        return Fail("DelayLineTest", "Expected initial read of 0.");

    delay.write(123.0f);
    x = delay.readForward(0);
    if (x != 123.0f)
        return Fail("DelayLineTest", std::string("Second read: did not find expected value. Found: ") + std::to_string(x));

    delay.write(456.0f);
    x = delay.readForward(0);
    if (x != 456.0f)
        return Fail("DelayLineTest", "Third read: did not find expected value.");

    // Set a longer buffer length.
    size_t n = delay.setLength(5);
    if (n != 5)
        return Fail("DelayLineTest", "delay.setLength(5) did not return 5.");

    // Verify the length is coherent.
    if (delay.getLength() != 5)
        return Fail("DelayLineTest", "Did not read back length = 5.");

    delay.clear();
    for (int i = 1; i <= 5; ++i)
    {
        x = delay.readForward(0);
        if (x != 0.0f)
            return Fail("DelayLineTest", std::string("i=") + std::to_string(i) + ": did not read zero.");

        delay.write(static_cast<float>(i));
    }

    for (size_t offset = 0; offset < 5; ++offset)
    {
        x = delay.readForward(offset);
        if (x != static_cast<float>(offset+1))
            return Fail("DelayLineTest", "Incorrect result returned by readForward.");

        x = delay.readBackward(offset);
        if (x != static_cast<float>(5-offset))
            return Fail("DelayLineTest", "Incorrect result returned by readBackward.");
    }

    for (int i = 1; i <= 5; ++i)
    {
        x = delay.readForward(0);
        if (x != static_cast<float>(i))
            return Fail("DelayLineTest", std::string("i=") + std::to_string(i) + ": did not read back i value.");

        delay.write(0.0f);
    }

    // Verify that invalid lengths get clamped.
    n = delay.setLength(0);
    if (n != 1)
        return Fail("DelayLineTest", "delay.setLength(0) did not return 1  -- clamp failure.");

    if (delay.getLength() != 1)
        return Fail("DelayLineTest", "delay.getLength() != 1 after delay.setLength(0) -- clamp failure.");

    n = delay.setLength(m + 1);
    if (n != m)
        return Fail("DelayLineTest", "delay.setLength(m+1) did return m -- clamp failure.");

    if (delay.getLength() != m)
        return Fail("DelayLineTest", "delay.getLength() != m after delay.setLength(m+1) -- clamp failure.");

    return Pass("DelayLineTest");
}


static int InterpolatorTest()
{
    using namespace Sapphire;

    // Verify the Blackman function is working as expected.
    // It is necessary for the Interpolator's functioning.
    double y = Blackman(0.0);
    if (std::abs(y) > 1.0e-7)
        return Fail("InterpolatorTest", std::string("Expected Blackman(0.0) = 0.0, but found ") + std::to_string(y));

    y = Blackman(0.5);
    if (std::abs(y-1.0) > 1.0e-7)
        return Fail("InterpolatorTest", std::string("Expected Blackman(0.5) = 1.0, but found ") + std::to_string(y));

    y = Blackman(1.0);
    if (std::abs(y) > 1.0e-7)
        return Fail("InterpolatorTest", std::string("Expected Blackman(1.0) = 0.0, but found ") + std::to_string(y));

    Interpolator<float, 5> interp;
    interp.write(-5, 1.0f);
    interp.write(-4, 2.0f);
    interp.write(-3, 3.0f);
    interp.write(-2, 4.0f);
    interp.write(-1, 5.0f);
    interp.write( 0, 6.0f);
    interp.write(+1, 5.0f);
    interp.write(+2, 4.0f);
    interp.write(+3, 3.0f);
    interp.write(+4, 2.0f);
    interp.write(+5, 1.0f);

    // Verify the 3 central integer offsets interpolate exactly.
    float x = interp.read(-1.0);
    float diff = std::abs(x - 5.0f);
    if (diff > 1.0e-6)
    {
        fprintf(stderr, "InterpolatorTest: interp.read(-1.0) excessive error: %e\n", diff);
        return 1;
    }

    x = interp.read(0.0);
    diff = std::abs(x - 6.0f);
    if (diff > 1.0e-6)
    {
        fprintf(stderr, "InterpolatorTest: interp.read(0.0) excessive error: %e\n", diff);
        return 1;
    }

    x = interp.read(+1.0);
    diff = std::abs(x - 5.0f);
    if (diff > 1.0e-6)
    {
        fprintf(stderr, "InterpolatorTest: interp.read(+1.0) excessive error: %e\n", diff);
        return 1;
    }

#if 0
    // Dump intermediate values for manual inspection.
    for (double position = -1.0; position <= +1.005; position += 0.01)
    {
        if (position > 1.0) position = 1.0;
        x = interp.read(position);
        printf("InterpolatorTest: position = %0.3lf, x = %0.6f\n", position, x);
    }
#endif

    return Pass("InterpolatorTest");
}


static int TaperTest()
{
    using namespace Sapphire;

    // Verify the accuracy of the sinc window optimization.
    // Compare the original `SlowTaper` calculation with the
    // fast approximation in `InterpolatorTable::Taper`.

    const size_t nsteps = 5;
    const size_t nsegments = 0x8001;
    InterpolatorTable table {nsteps, nsegments};

    const float limit = static_cast<float>(nsteps + 1);
    const float increment = 8.675309e-6f;
    float sum = 0.0f;
    float maxdy = 0.0f;
    int n = 0;
    for (float x = -limit; x <= +limit; x += increment)
    {
        float y1 = SlowTaper(x, nsteps);
        float y2 = table.Taper(x);
        float dy = y1 - y2;
        maxdy = std::max(maxdy, dy);
        sum += (dy * dy);
        ++n;
    }

    float dev = std::sqrt(sum / n);
    printf("TaperTest: n = %d, standard deviation = %0.4e, max error = %0.4e\n", n, dev, maxdy);
    if (dev > 2.71e-8f)
        return Fail("TaperTest", "Excessive error standard deviation.");

    if (maxdy > 2.39e-7f)
        return Fail("TaperTest", "Excessive maximum error.");

    return Pass("TaperTest");
}


static int QuadEndpointCheck(float Q, float R, float S)
{
    using namespace Sapphire;

    float q = QuadInterp(Q, R, S, -1.0f);
    float r = QuadInterp(Q, R, S,  0.0f);
    float s = QuadInterp(Q, R, S, +1.0f);

    float error = std::max({std::abs(Q-q), std::abs(R-r), std::abs(S-s)});
    if (error > 0.0f)
    {
        fprintf(stderr, "QuadEndpointCheck(%f, %f, %f): EXCESSIVE ERROR %e\n", Q, R, S, error);
        return 1;
    }
    return 0;
}


static int QuadraticTest()
{
    using namespace Sapphire;

    // Verify the quadratic interpolator hits the endpoints of a parabola.
    if (QuadEndpointCheck(-3.0f, +4.0f, -7.0f))
        return 1;

    return Pass("QuadraticTest");
}


static int CheckLimits(const Sapphire::ChaoticOscillator& osc)
{
    double x = osc.vx();
    double y = osc.vy();
    double z = osc.vz();
    if (!std::isfinite(x) || std::abs(x) > Sapphire::CHAOS_AMPLITUDE)
    {
        printf("x is out of bounds: %lg\n", x);
        return 1;
    }
    if (!std::isfinite(y) || std::abs(y) > Sapphire::CHAOS_AMPLITUDE)
    {
        printf("y is out of bounds: %lg\n", y);
        return 1;
    }
    if (!std::isfinite(z) || std::abs(z) > Sapphire::CHAOS_AMPLITUDE)
    {
        printf("z is out of bounds: %lg\n", z);
        return 1;
    }
    return 0;
}


static int RangeTest(Sapphire::ChaoticOscillator& osc, const char *name)
{
    printf("RangeTest(%s): starting\n", name);

    const long SAMPLE_RATE = 44100;
    const long SIM_SECONDS = 6 * 3600;
    const long SIM_SAMPLES = SIM_SECONDS * SAMPLE_RATE;
    const double dt = 1.0 / SAMPLE_RATE;

    double xMin = 0;
    double xMax = 0;
    double yMin = 0;
    double yMax = 0;
    double zMin = 0;
    double zMax = 0;

    const long SETTLE_SECONDS = 60;
    const long SETTLE_SAMPLES = SETTLE_SECONDS * SAMPLE_RATE;
    for (long i = 0; i < SETTLE_SAMPLES; ++i)
    {
        osc.update(dt);
        if (CheckLimits(osc)) return 1;
    }

    for (long i = 0; i < SIM_SAMPLES; ++i)
    {
        osc.update(dt);
        if (CheckLimits(osc)) return 1;
        if (i == 0)
        {
            xMin = xMax = osc.vx();
            yMin = yMax = osc.vy();
            zMin = zMax = osc.vz();
        }
        else
        {
            xMin = std::min(xMin, osc.vx());
            xMax = std::max(xMax, osc.vx());
            yMin = std::min(yMin, osc.vy());
            yMax = std::max(yMax, osc.vy());
            zMin = std::min(zMin, osc.vz());
            zMax = std::max(zMax, osc.vz());
        }
    }

    printf("RangeTest(%s): finished\n", name);
    printf("vx range: %10.6lf %10.6lf\n", xMin, xMax);
    printf("vy range: %10.6lf %10.6lf\n", yMin, yMax);
    printf("vz range: %10.6lf %10.6lf\n", zMin, zMax);

    return 0;
}


static int ChaosTest()
{
    Sapphire::Rucklidge ruck;
    ruck.setKnob(+1.0);     // maximum chaos and maximum range

    Sapphire::Aizawa aiza;

    return
        RangeTest(ruck, "Rucklidge") ||
        RangeTest(aiza, "Aizawa") ||
        Pass("ChaosTest");
}


class IdentityBlockHandler : public Sapphire::BlockHandler<float>
{
public:
    void initialize() override
    {
        // Nothing to do.
    }

    void onBlock(int length, const float* inBlock, float* outBlock) override
    {
        for (int i = 0; i < length; ++i)
            outBlock[i] = inBlock[i];
    }
};


static int Dump(
    const char *outFileName,
    const std::vector<float>& buffer,
    int granuleSize)
{
    FILE *outfile = fopen(outFileName, "wt");
    if (outfile == nullptr)
    {
        printf("Dump: cannot open output file: %s\n", outFileName);
        return 1;
    }

    const int n = static_cast<int>(buffer.size());
    for (int i = 0; i < n; ++i)
    {
        if ((i > 0) && (granuleSize > 0) && (i % granuleSize == 0))
            fprintf(outfile, "\n");
        fprintf(outfile, "[%6d] %12.6f\n", i, buffer[i]);
    }

    printf("Dump: Wrote file %s\n", outFileName);
    fclose(outfile);
    return 0;
}


static int VerifyZeroGranule(const std::vector<float>& yhist, int granuleIndex, int granuleSize)
{
    int offset = granuleSize * granuleIndex;
    for (int i = 0; i < granuleSize; ++i)
        if (yhist.at(i + offset) != 0)
            return Fail("VerifyZeroGranule", "Value is not zero");

    return 0;
}


static int VerifySameGranule(
    const std::vector<float>& yhist,
    int yIndex,
    const std::vector<float>& xhist,
    int xIndex,
    int granuleSize)
{
    const float TOLERANCE = 1.0e-6;
    int yOffset = yIndex * granuleSize;
    int xOffset = xIndex * granuleSize;
    for (int i = 0; i < granuleSize; ++i)
    {
        float delta = yhist.at(yOffset + i) - xhist.at(xOffset + i);
        float diff = std::abs(delta);
        if (diff > TOLERANCE)
            return Fail("VerifySameGranule", std::string("EXCESSIVE delta = ") + std::to_string(delta));
    }
    return 0;
}


static int GranuleTest_Identity()
{
    using namespace Sapphire;

    const float SAMPLERATE = 48000;
    const int GRANULE_SIZE = 8;
    const float TOLERANCE = 0;
    IdentityBlockHandler ident;
    GranularProcessor<float> gran(GRANULE_SIZE, ident);
    RandomVectorGenerator r;
    std::vector<float> xhist;
    std::vector<float> yhist;

    // Generate random values from a normal distribution.
    // After feeding 3 granules of random data, we should have
    // created 2 input blocks that are processed to produce 2 procblocks.
    // The next granule we read should be the overlapping part of those two procblocks
    // crossfaded back to the original input, but delayed by 2 granules (1 block).

    for (int i = 0; i < 5*GRANULE_SIZE; ++i)
    {
        float x = r.next();
        xhist.push_back(x);
        float y = gran.process(x, SAMPLERATE);
        yhist.push_back(y);
    }

    // Dump the output for analysis and verification...

    const std::vector<float>& fade = gran.crossfadeBuffer();

    if (Dump("output/granule_identity_x.txt", xhist, GRANULE_SIZE)) return 1;
    if (Dump("output/granule_identity_y.txt", yhist, GRANULE_SIZE)) return 1;
    if (Dump("output/granule_identity_fade.txt", fade, 0)) return 1;

    // Verify the fade buffer fades from 1 to 0 in a symmetric way.
    for (int i = 0; i < GRANULE_SIZE / 2; ++i)
    {
        float sum = fade.at(i) + fade.at((GRANULE_SIZE-1) - i);
        float diff = std::abs(sum - 1);
        if (diff > TOLERANCE)
            return Fail("GranuleTest_Identity", std::string("Crossfade buffer error = ") + std::to_string(diff) + " at sample " + std::to_string(i));
    }

    // The output should be:
    // granule[0] = silence (all zero)
    // granule[1] = silence (all zero)
    // granule[2] = input[0]
    // granule[3] = input[1]
    // granule[4] = input[2]
    // ...

    if (VerifyZeroGranule(yhist, 0, GRANULE_SIZE)) return 1;
    if (VerifyZeroGranule(yhist, 1, GRANULE_SIZE)) return 1;
    if (VerifySameGranule(yhist, 2, xhist, 0, GRANULE_SIZE)) return 1;
    if (VerifySameGranule(yhist, 3, xhist, 1, GRANULE_SIZE)) return 1;
    if (VerifySameGranule(yhist, 4, xhist, 2, GRANULE_SIZE)) return 1;

    return Pass("GranuleTest_Identity");
}


static int GranuleTest_Reverse()
{
    class ReverseBlockHandler : public Sapphire::BlockHandler<float>
    {
    public:
        void initialize() override
        {
            // Nothing to do.
        }

        void onBlock(int length, const float* inBlock, float* outBlock) override
        {
            for (int i = 0; i < length; ++i)
                outBlock[i] = inBlock[(length-1)-i];
        }
    };

    const int GRANULE_SIZE = 6000;
    ReverseBlockHandler reverser;
    Sapphire::GranularProcessor<float> gran{GRANULE_SIZE, reverser};

    const char *inFileName = MyVoiceFileName;
    const char *outFileName = "output/granular_reverse_genesis.wav";

    WaveFileReader inwave;
    if (!inwave.Open(inFileName))
        return Fail("GranuleTest_Reverse", std::string("Could not open input file: ") + inFileName);

    int sampleRate = inwave.SampleRate();
    int channels = inwave.Channels();
    if (sampleRate != 44100 || channels != 2)
    {
        fprintf(stderr, "GranuleTest_Reverse: FAIL - Expected 44100 Hz stereo, but found %d Hz, %d channels.\n", sampleRate, channels);
        return 1;
    }

    WaveFileWriter outwave;
    if (!outwave.Open(outFileName, sampleRate, channels))
        return Fail("GranuleTest_Reverse", std::string("Could not open output file: ") + outFileName);

    std::vector<float> buffer;
    buffer.resize(GRANULE_SIZE);

    for(;;)
    {
        size_t received = inwave.Read(buffer.data(), GRANULE_SIZE);
        for (size_t i = 0; i < received; ++i)
            buffer[i] = gran.process(buffer[i], sampleRate);
        outwave.WriteSamples(buffer.data(), received);
        if (received < GRANULE_SIZE)
            break;
    }

    // Write 2 extra granules worth of silence at the end,
    // to force out the end of our signal.
    // Every granulized signal ends up delayed by 2 granules worth of time.
    for (int i = 0; i < 2*GRANULE_SIZE; ++i)
    {
        float x = 0;
        float y = gran.process(x, sampleRate);
        outwave.WriteSamples(&y, 1);
    }
    return Pass("GranuleTest_Reverse");
}


class BandpassFilter : public Sapphire::FourierFilter<float>
{
public:
    BandpassFilter(int granuleExponent)
        : Sapphire::FourierFilter<float>(granuleExponent)
        {}

    void initialize() override
    {
    }

    void onSpectrum(int length, const float* inSpectrum, float* outSpectrum) override
    {
        for (int i = 0; i < length; ++i)
            outSpectrum[i] = inSpectrum[i];     // FIXFIXFIX: simple identity function, not actual bandpass yet
    }
};


static int GranuleTest_FFT_Bandpass()
{
    // FIXFIXFIX: FFT filter should be given exponent for a BLOCK = 2*GRANULE, not a GRANULE.
    // All of this is tricky, and there should be a single source of truth. Therefore,
    // provide some kind of helper function to set both block exponent and granule size?
    const int GRANULE_EXPONENT = 14;
    const int GRANULE_SIZE = 1 << GRANULE_EXPONENT;    // 16K = 2^14
    BandpassFilter fourier{GRANULE_EXPONENT};
    Sapphire::GranularProcessor<float> gran{GRANULE_SIZE, fourier};
    return Pass("GranuleTest_FFT_Bandpass");
}


static int GranuleTest()
{
    return
        GranuleTest_Identity() ||
        GranuleTest_Reverse() ||
        GranuleTest_FFT_Bandpass() ||
        Pass("GranuleTest");
}
