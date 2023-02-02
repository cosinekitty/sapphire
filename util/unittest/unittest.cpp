#include <cstdio>
#include <cstring>
#include <random>
#include "sapphire_engine.hpp"
#include "wavefile.hpp"

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
static int ReadWave();
static int AutoScale();
static int DelayLineTest();
static int InterpolatorTest();

static const UnitTest CommandTable[] =
{
    { "agc",        AutoGainControl },
    { "delay",      DelayLineTest },
    { "interp",     InterpolatorTest },
    { "readwave",   ReadWave },
    { "scale",      AutoScale },
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
    const char *inFileName = "input/genesis.wav";
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
    if (std::abs(y) > 1.0e-12)
        return Fail("InterpolatorTest", std::string("Expected Blackman(0.0) = 0.0, but found ") + std::to_string(y));

    y = Blackman(0.5);
    if (std::abs(y-1.0) > 1.0e-12)
        return Fail("InterpolatorTest", std::string("Expected Blackman(0.5) = 1.0, but found ") + std::to_string(y));

    y = Blackman(1.0);
    if (std::abs(y) > 1.0e-12)
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
        return Fail("InterpolatorTest", std::string("interp.read(-1.0) excessive error: ") + std::to_string(diff));

    x = interp.read(0.0);
    diff = std::abs(x - 6.0f);
    if (diff > 1.0e-6)
        return Fail("InterpolatorTest", std::string("interp.read(0.0) excessive error: ") + std::to_string(diff));

    x = interp.read(+1.0);
    diff = std::abs(x - 5.0f);
    if (diff > 1.0e-6)
        return Fail("InterpolatorTest", std::string("interp.read(+1.0) excessive error: ") + std::to_string(diff));

    // Dump intermediate values for manual inspection.
    for (double position = -1.0; position <= +1.005; position += 0.01)
    {
        if (position > 1.0) position = 1.0;
        x = interp.read(position);
        printf("InterpolatorTest: position = %0.3lf, x = %0.6f\n", position, x);
    }

    return Pass("InterpolatorTest");
}

