/*
    elastika_standalone.cpp  -  Don Cross <cosinekitty@gmail.com>

    Demo of using the Elastika engine completely outside of VCV Rack.
*/

#include <string>
#include <vector>
#include "elastika_engine.hpp"
#include "wavefile.hpp"

int main()
{
    using namespace std;
    using namespace Sapphire;

    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 2;
    const int DURATION_SECONDS = 10;
    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;
    const int FADE_SECONDS = 7;
    const int FADE_SAMPLES = SAMPLE_RATE * FADE_SECONDS;

    ElastikaEngine engine;
    engine.setAgcEnabled(false);
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

    WaveFileWriter wave;
    const char *filename = "elastika.wav";
    if (!wave.Open(filename, SAMPLE_RATE, CHANNELS))
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", filename);
        return 1;
    }

    float sample[CHANNELS];

    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        engine.process(SAMPLE_RATE, 0.0f, 0.0f, sample[0], sample[1]);
        wave.WriteSamples(sample, CHANNELS);
        if (s == FADE_SAMPLES)
        {
            engine.setFriction(0.46f);
            engine.setCurl(0.0f);
        }
    }

    return 0;
}
