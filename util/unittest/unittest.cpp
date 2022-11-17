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
private:
    std::mt19937 rand;
    double rspan;
    double rmid;
    Sapphire::StagedFilter<3> lo;
    Sapphire::StagedFilter<3> hi;
    double amplitude;
    double sampleRate;

public:
    TestSignal(unsigned _seed, double _amplitude, double _sampleRate)
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


static int AutoGainControl()
{
    const double ceiling = 1.0;
    const double amplitude = 10.0;
    Sapphire::AutomaticGainLimiter agc { ceiling, 0.005, 0.05 };

    const int sampleRate = 44100;
    const int durationSeconds = 5;
    const int durationSamples = sampleRate * durationSeconds;

    TestSignal leftSignal  {0x539a0c27, amplitude, sampleRate};
    TestSignal rightSignal {0x7ac5b398, amplitude, sampleRate};

    WaveFile harsh;
    const char *harshFileName = "agc_input.wav";
    if (!harsh.Open(harshFileName, sampleRate, 2))
    {
        fprintf(stderr, "AutoGainControl: Cannot open output wave file: %s\n", harshFileName);
        return 1;
    }

    WaveFile mild;
    const char *mildFileName = "agc_output.wav";
    if (!mild.Open(mildFileName, sampleRate, 2))
    {
        fprintf(stderr, "AutoGainControl: Cannot open output wave file: %s\n", mildFileName);
        return 1;
    }

    // Burn a few samples to get the filters to settle down.
    for (int i = 0; i < 10000; ++i)
    {
        leftSignal.getSample();
        rightSignal.getSample();
    }

    float minHarsh = 0.0f;
    float maxHarsh = 0.0f;
    float maxMild = 0.0f;
    float sample[2];
    for (int i = 0; i < durationSamples; ++i)
    {
        float left  = leftSignal.getSample();
        float right = rightSignal.getSample();

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

    printf("AutoGainControl: minHarsh = %0.6f, maxHarsh = %0.6f\n", minHarsh, maxHarsh);
    printf("AutoGainControl: maxMild = %0.6f\n", maxMild);

    const double ideal = amplitude / ceiling;
    const double overshoot = ideal / agc.getFollower();
    printf("AutoGainControl: ideal follower = %0.6lf, actual follower = %0.6lf, overshoot = %0.6lf\n", ideal, agc.getFollower(), overshoot);

    int error = 0;

    if (maxHarsh < 9.981 || maxHarsh > 9.982)
    {
        printf("AutoGainControl FAIL: maxHarsh was out of bounds.\n");
        error = 1;
    }

    if (overshoot < 0.999 || overshoot > 1.000)
    {
        printf("AutoGainControl FAIL: overshoot was out of bounds.\n");
        error = 1;
    }

    if (error == 0)
        printf("AutoGainControl: PASS\n");

    return error;
}
