#include <cstdio>
#include <cstring>
#include "sapphire_engine.hpp"

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


static int AutoGainControl()
{
    Sapphire::AutomaticGainLimiter agc { 5.0f, 0.01f, 1.0f };

    float left = 0.0f;
    float right = 0.0f;

    for (int i = 0; i < 44100; ++i)
    {
        right = 10.0f * cos((i/44100.0)*440.0 * (2.0 * M_PI));
        agc.process(44100, left, right);
    }

    float diff = std::abs(right - 5.152809f);
    if (diff > 1.0e-6)
    {
        printf("AutoGainControl FAIL: right = %f, diff = %f\n", right, diff);
        return 1;
    }

    printf("AutoGainControl: PASS\n");
    return 0;
}
