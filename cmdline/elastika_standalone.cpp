/*
    elastika_standalone.cpp  -  Don Cross <cosinekitty@gmail.com>

    Demo of using the Elastika engine completely outside of VCV Rack.
*/

#include <cinttypes>
#include <cstdio>
#include <string>
#include "elastika_engine.hpp"

inline int16_t ConvertSample(float x)
{
    if (x < -1.0f || x > +1.0f)
        throw std::range_error("Elastika audio output went out of range.");

    return static_cast<int16_t>(32700.0 * x);
}

int main()
{
    using namespace std;
    using namespace Sapphire;

    int error = 1;

    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 2;
    const int DURATION_SECONDS = 10;
    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;
    const int FADE_SECONDS = 7;
    const int FADE_SAMPLES = SAMPLE_RATE * FADE_SECONDS;

    ElastikaEngine engine;
    engine.setDcRejectFrequency(80.0);
    engine.setFriction(0.25379982590675354);
    engine.setStiffness(0.64640343189239502);
    engine.setSpan(0.5331994891166687);
    engine.setCurl(0.37039878964424133);
    engine.setMass(-0.62280040979385376);
    engine.setDrive(1.0);
    engine.setGain(1.06);
    engine.setInputTilt(0.5);
    engine.setOutputTilt(0.5);

    const char *filename = "elastika.wav";
    FILE *outfile = fopen(filename, "wb");
    if (!outfile)
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", filename);
        return 1;
    }

    float sample[CHANNELS];
    const int BUFFER_SAMPLES = 10000;
    const int BUFFER_DATA = CHANNELS * BUFFER_SAMPLES;
    vector<int16_t> data(BUFFER_DATA);
    size_t buf = 0;

    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        engine.process(SAMPLE_RATE, 0.0f, 0.0f, sample[0], sample[1]);

        data[buf++] = ConvertSample(sample[0]);
        data[buf++] = ConvertSample(sample[1]);
        if ((buf == BUFFER_DATA) || (s == DURATION_SAMPLES-1))
        {
            size_t wrote = fwrite(data.data(), sizeof(int16_t), buf, outfile);
            if (wrote < buf)
            {
                fprintf(stderr, "ERROR: Could not write data to file: %s\n", filename);
                goto fail;
            }
            buf = 0;
        }

        if (s == FADE_SAMPLES)
        {
            engine.setFriction(0.46f);
            engine.setCurl(0.0f);
        }
    }

    error = 0;
fail:
    fclose(outfile);
    return error;
}
