/*
    Don Cross <cosinekitty@gmail.com>

    Demo of using the Tube Monster engine.
*/

#include <string>
#include <vector>
#include "tubemonster_engine.hpp"
#include "wavefile.hpp"

int main()
{
    using namespace std;
    using namespace Sapphire;

    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 2;
    const int DURATION_SECONDS = 10;
    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;

    TubeMonsterEngine engine;
    engine.setSampleRate(SAMPLE_RATE);

    ScaledWaveFileWriter wave;
    const char *filename = "test/tubemonster.wav";
    if (!wave.Open(filename, SAMPLE_RATE, CHANNELS))
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", filename);
        return 1;
    }

    float sample[CHANNELS];
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        // FIXFIXFIX - Update engine parameters.
        // FIXFIXFIX - Supply stereo input.
        engine.process(sample[0], sample[1], 0.0f, 0.0f);
        wave.WriteSamples(sample, CHANNELS);
    }

    return 0;
}
