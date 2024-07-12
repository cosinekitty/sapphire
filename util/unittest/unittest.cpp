#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include "sapphire_engine.hpp"
#include "galaxy_engine.hpp"
#include "obelisk_engine.hpp"
#include "wavefile.hpp"
#include "chaos.hpp"
#include "Galactic.h"

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
static int ChaosTest();
static int ReadWave();
static int AutoScale();
static int DelayLineTest();
static int InterpolatorTest();
static int TaperTest();
static int QuadraticTest();
static int GalaxyTest();
static int ObeliskTest();
static int PivotTest();

static const UnitTest CommandTable[] =
{
    { "agc",        AutoGainControl     },
    { "chaos",      ChaosTest           },
    { "delay",      DelayLineTest       },
    { "galaxy",     GalaxyTest          },
    { "interp",     InterpolatorTest    },
    { "obelisk",    ObeliskTest         },
    { "quad",       QuadraticTest       },
    { "readwave",   ReadWave            },
    { "scale",      AutoScale           },
    { "pivot",      PivotTest           },
    { "taper",      TaperTest           },
    { nullptr,      nullptr             }
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


static int GalaxyTest_Genesis()
{
    const char *inFileName = "input/genesis.wav";
    const char *outFileName = "output/galaxy_genesis.wav";
    const int sampleRate = 44100;
    const int channels = 2;

    WaveFileReader inwave;
    if (!inwave.Open(inFileName))
        return Fail("GalaxyTest_Genesis", std::string("Could not open input file: ") + inFileName);

    ScaledWaveFileWriter outwave;
    if (!outwave.Open(outFileName, sampleRate, channels))
        return Fail("GalaxyTest_Genesis", std::string("Could not open output file: ") + outFileName);

    Sapphire::Galaxy::Engine engine;
    engine.setReplace(0.5);
    engine.setBrightness(0.5);
    engine.setDetune(0.5);
    engine.setBigness(1.0);
    engine.setMix(1.0);

    float inFrame[channels]{};
    float outFrame[channels]{};

    while (inwave.Read(inFrame, channels) == channels)
    {
        engine.process(sampleRate, inFrame[0], inFrame[1], outFrame[0], outFrame[1]);
        outwave.WriteSamples(outFrame, channels);
    }

    const int flushSeconds = 6;
    const int flushFrames = sampleRate * flushSeconds;
    for (int frame = 0; frame < flushFrames; ++frame)
    {
        engine.process(sampleRate, 0, 0, outFrame[0], outFrame[1]);
        outwave.WriteSamples(outFrame, channels);
    }

    outwave.Close();
    inwave.Close();
    return Pass("GalaxyTest_Genesis");
}


static int GalaxyTest_OriginalGenesis()
{
    const char *inFileName = "input/genesis.wav";
    const char *outFileName = "output/airwindows_genesis.wav";
    const int sampleRate = 44100;
    const int channels = 2;

    WaveFileReader inwave;
    if (!inwave.Open(inFileName))
        return Fail("GalaxyTest_OriginalGenesis", std::string("Could not open input file: ") + inFileName);

    ScaledWaveFileWriter outwave;
    if (!outwave.Open(outFileName, sampleRate, channels))
        return Fail("GalaxyTest_OriginalGenesis", std::string("Could not open output file: ") + outFileName);

    float inFrame[channels]{};
    float outFrame[channels]{};

    float *inputs[channels] = {&inFrame[0], &inFrame[1]};
    float *outputs[channels] = {&outFrame[0], &outFrame[1]};
    auto galactic = std::make_unique<Galactic>();
    galactic->setParameter(0, 0.5);
    galactic->setParameter(1, 0.5);
    galactic->setParameter(2, 0.5);
    galactic->setParameter(3, 1.0);
    galactic->setParameter(4, 1.0);

    while (inwave.Read(inFrame, channels) == channels)
    {
        galactic->process(sampleRate, inputs, outputs, 1);
        outwave.WriteSamples(outFrame, channels);
    }

    const int flushSeconds = 6;
    const int flushFrames = sampleRate * flushSeconds;
    inFrame[0] = inFrame[1] = 0;
    for (int frame = 0; frame < flushFrames; ++frame)
    {
        galactic->process(sampleRate, inputs, outputs, 1);
        outwave.WriteSamples(outFrame, channels);
    }

    outwave.Close();
    inwave.Close();
    return Pass("GalaxyTest_OriginalGenesis");
}


static int GalaxyTest()
{
    return
        GalaxyTest_OriginalGenesis() ||
        GalaxyTest_Genesis() ||
        Pass("GalaxyTest");
}


static int ObeliskTest()
{
    using namespace Sapphire;

    const char *outFileName = "output/obelisk.wav";
    ScaledWaveFileWriter outwave;
    const int sampleRate = 44100;
    const int channels = 2;
    if (!outwave.Open(outFileName, sampleRate, channels))
        return Fail("ObeliskTest", std::string("Could not open output file: ") + outFileName);

    const int nparticles = 16;
    Obelisk::Engine<nparticles> engine;

    const int outputDurationSeconds = 5;
    const int outputFrames = sampleRate * outputDurationSeconds;
    float inFrame[2];
    float outFrame[2];

    inFrame[0] = +7;
    inFrame[1] = -3;
    engine.process(sampleRate, inFrame[0], inFrame[1], outFrame[0], outFrame[1]);
    inFrame[0] = inFrame[1] = 0;

    for (int frame = 0; frame < outputFrames; ++frame)
    {
        engine.process(sampleRate, inFrame[0], inFrame[1], outFrame[0], outFrame[1]);
        outwave.WriteSamples(outFrame, channels);
    }

    outwave.Close();
    return Pass("ObeliskTest");
}


static int PivotTest()
{
    using namespace Sapphire;

    const float tolerance = 1.1e-7;

    // Verify that 2pi*n radians selects from n=0 for x, n=1 for y, n=2 for z.

    PhysicsVector xAxis = Obelisk::PivotAxis(0);
    float dx = xAxis[0] - 1;
    float dy = xAxis[1] - 0;
    float dz = xAxis[2] - 0;
    float ds = std::sqrt(dx*dx + dy*dy + dz*dz);
    printf("PivotTest: X ds = %g\n", ds);
    if (ds > tolerance)
        return Fail("PivotTest", "EXCESSIVE X-AXIS ERROR (ds)");

    PhysicsVector yAxis = Obelisk::PivotAxis(1);
    dx = yAxis[0] - 0;
    dy = yAxis[1] - 1;
    dz = yAxis[2] - 0;
    ds = std::sqrt(dx*dx + dy*dy + dz*dz);
    printf("PivotTest: Y ds = %g\n", ds);
    if (ds > tolerance)
        return Fail("PivotTest", "EXCESSIVE Y-AXIS ERROR (ds)");

    PhysicsVector zAxis = Obelisk::PivotAxis(2);
    dx = zAxis[0] - 0;
    dy = zAxis[1] - 0;
    dz = zAxis[2] - 1;
    ds = std::sqrt(dx*dx + dy*dy + dz*dz);
    printf("PivotTest: Z ds = %g\n", ds);
    if (ds > tolerance)
        return Fail("PivotTest", "EXCESSIVE Z-AXIS ERROR (ds)");

    PhysicsVector vec = Obelisk::PivotAxis(0.5f);
    printf("PivotTest: vec = (%0.6f, %0.6f, %0.6f)\n", vec[0], vec[1], vec[2]);

    return Pass("PivotTest");
}
