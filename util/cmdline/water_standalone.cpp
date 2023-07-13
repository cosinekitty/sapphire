/*
    water_standalone.cpp  -  Don Cross  -  2023-07-11
*/

#include <cstdio>
#include "water_engine.hpp"
#include "wavefile.hpp"

const int STEPS  = 100000;
const int CHANNELS = 2;
const int SAMPLE_RATE = 48000;
const float DURATION_SECONDS = 7.0f;
const int DURATION_FRAMES = static_cast<int>(SAMPLE_RATE * DURATION_SECONDS);
const float dt = 1.0f / SAMPLE_RATE;
const float halflife = 0.3f;
const float c = 0.7f;             // speed of waves in meters/second
const float s = 0.001f;           // grid spacing in meters
const float k = (c*c) / (s*s);    // propagation constant [second^(-2)]

int main()
{
    using namespace Sapphire;

    const char *outFileName = "test/waterpool.wav";

    ScaledWaveFileWriter outwave;
    if (!outwave.Open(outFileName, SAMPLE_RATE, CHANNELS))
    {
        printf("Cannot open output wave file: %s\n", outFileName);
        return 1;
    }

    WaterEngine engine;
    engine.setHalfLife(halflife);
    engine.setPropagation(k);

    float input[CHANNELS] = {0.0f, 0.0f};
    float sample[CHANNELS];
    for (int frame = 0; frame < DURATION_FRAMES; ++frame)
    {
        if (frame == 100)
        {
            input[0] = +1.0f;
            input[1] = -1.0f;
        }
        else
        {
            input[0] = 0.0f;
            input[1] = 0.0f;
        }

        engine.process(dt, sample[0], sample[1], input[0], input[1]);
        outwave.WriteSamples(sample, CHANNELS);
    }

    outwave.Close();
    printf("Wrote: %s\n", outFileName);
    return 0;
}
