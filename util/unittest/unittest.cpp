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

static const UnitTest CommandTable[] =
{
    { "agc",        AutoGainControl },
    { "readwave",   ReadWave },
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
    Sapphire::StagedFilter<3> lo;
    Sapphire::StagedFilter<3> hi;
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
