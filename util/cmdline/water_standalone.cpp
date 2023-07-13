/*
    water_standalone.cpp  -  Don Cross  -  2023-07-11
*/

#include <cstdio>
#include "waterpool.hpp"
#include "wavefile.hpp"

const int WIDTH  = 120;
const int HEIGHT =  50;

using pool_t = Sapphire::WaterPoolSimd<WIDTH, HEIGHT>;

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
    if (!outwave.Open(outFileName, SAMPLE_RATE, 2))
    {
        printf("Cannot open output wave file: %s\n", outFileName);
        return 1;
    }

    pool_t pool;
    pool.putPos(WIDTH-10, 13, 1.0f);

    float sample[CHANNELS];
    for (int frame = 0; frame < DURATION_FRAMES; ++frame)
    {
        // Produce stereo output by listening to a pair of locations.
        sample[0] = pool.get(30, 43).pos;
        sample[1] = pool.get(31, 47).pos;
        outwave.WriteSamples(sample, CHANNELS);
        pool.update(dt, halflife, k);
    }

    outwave.Close();
    printf("Wrote: %s\n", outFileName);
    return 0;
}
