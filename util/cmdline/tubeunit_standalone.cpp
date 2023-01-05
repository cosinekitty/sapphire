/*
    tubeunit_standalone.cpp  -  Don Cross <cosinekitty@gmail.com>

    Demo of using the TubeUnit engine completely outside of VCV Rack.
*/

#include <string>
#include <vector>
#include "tubeunit_engine.hpp"
#include "wavefile.hpp"

int main()
{
    using namespace std;
    using namespace Sapphire;

    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 2;
    const int DURATION_SECONDS = 10;
    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;

    TubeUnitEngine engine;
    engine.setSampleRate(SAMPLE_RATE);

    ScaledWaveFileWriter wave;
    const char *filename = "test/tubeunit.wav";
    if (!wave.Open(filename, SAMPLE_RATE, CHANNELS))
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", filename);
        return 1;
    }

    float sample[CHANNELS];
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        // Apply airflow from 0.2 seconds until 8.0 seconds.
        if (s >= DURATION_SAMPLES/50 && s <= (DURATION_SAMPLES*8)/10)
            engine.setAirflow(1.0f);
        else
            engine.setAirflow(0.0f);

        engine.process(sample[0], sample[1]);
        wave.WriteSamples(sample, CHANNELS);
    }

    return 0;
}
