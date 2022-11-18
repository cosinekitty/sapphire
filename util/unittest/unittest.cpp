#include <cstdio>
#include <cstring>
#include <random>
#include "sapphire_engine.hpp"
#include "wavefile.hpp"

using UnitTestFunction = int (*) ();

struct UnitTest
{
    const char *name;
    UnitTestFunction func;
};

static int AutoGainControl();

static const UnitTest CommandTable[] =
{
    { "agc",    AutoGainControl },
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
        return s * amplitude * 2.53;        // experimental fudge factor: compensates for filter losses
    }
};


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


static int AgcTestCase(
    const char *name,
    TestSignal& signal,
    int sampleRate,
    int durationSeconds)
{
    const double ceiling = 1.0;
    const double amplitude = 10.0;
    Sapphire::AutomaticGainLimiter agc { ceiling, 0.005, 0.05 };

    std::string harshFileName = std::string("agc_input_")  + name + ".wav";
    std::string mildFileName  = std::string("agc_output_") + name + ".wav";

    WaveFile harsh;
    if (!harsh.Open(harshFileName.c_str(), sampleRate, 2))
    {
        printf("AgcTestCase(%s): Cannot open output wave file: %s\n", name, harshFileName.c_str());
        return 1;
    }

    WaveFile mild;
    if (!mild.Open(mildFileName.c_str(), sampleRate, 2))
    {
        printf("AgcTestCase(%s): Cannot open output wave file: %s\n", name, mildFileName.c_str());
        return 1;
    }

    // Burn a few samples to get the filters to settle down.
    float left, right;
    for (int i = 0; i < 10000; ++i)
        signal.getSample(left, right);

    float minHarsh = 0.0f;
    float maxHarsh = 0.0f;
    float maxMild = 0.0f;
    float sample[2];
    const int durationSamples = sampleRate * durationSeconds;
    for (int i = 0; i < durationSamples; ++i)
    {
        signal.getSample(left, right);

        minHarsh = std::min(minHarsh, std::min(left, right));
        maxHarsh = std::max(maxHarsh, std::max(left, right));

        sample[0] = left / amplitude;
        sample[1] = right / amplitude;
        harsh.WriteSamples(sample, 2);

        agc.process(sampleRate, left, right);

        if (i > durationSamples / 4)
            maxMild = std::max(maxMild, std::max(std::abs(left), std::abs(right)));

        sample[0] = left / amplitude;
        sample[1] = right / amplitude;
        mild.WriteSamples(sample, 2);
    }

    printf("AgcTestCase(%s): minHarsh = %0.6f, maxHarsh = %0.6f\n", name, minHarsh, maxHarsh);
    printf("AgcTestCase(%s): maxMild = %0.6f\n", name, maxMild);

    const double ideal = amplitude / ceiling;
    const double overshoot = ideal / agc.getFollower();
    printf("AgcTestCase(%s): ideal follower = %0.6lf, actual follower = %0.6lf, overshoot = %0.6lf\n",
        name, ideal, agc.getFollower(), overshoot);

    int error = 0;

    if (maxHarsh < 9.981 || maxHarsh > 9.982)
    {
        printf("AgcTestCase(%s) FAIL: maxHarsh was out of bounds.\n", name);
        error = 1;
    }

    if (overshoot < 0.999 || overshoot > 1.000)
    {
        printf("AgcTestCase(%s) FAIL: overshoot was out of bounds.\n", name);
        error = 1;
    }

    if (error == 0)
        printf("AgcTestCase(%s): PASS\n", name);

    return error;
}


static int AutoGainControl()
{
    const int sampleRate = 44100;
    const int durationSeconds = 5;
    const double amplitude = 10.0;

    TestSignal_Random randomSignal { amplitude, sampleRate };
    if (AgcTestCase("random", randomSignal, sampleRate, durationSeconds)) return 1;
    return 0;
}
