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

    const int DURATION_SECONDS = 10;

    WaveFileReader inwave;
    const char *inWaveFileName = "input/DryGuitarForDon-16-bit.wav";
    if (!inwave.Open(inWaveFileName))
    {
        fprintf(stderr, "ERROR: Cannot open input file: %s\n", inWaveFileName);
        return 1;
    }

    const int CHANNELS = inwave.Channels();
    const int SAMPLE_RATE = inwave.SampleRate();
    printf("tubemonster(%s): Sample rate = %d, channels = %d\n", inWaveFileName, SAMPLE_RATE, CHANNELS);

    if (CHANNELS != 2)
    {
        fprintf(stderr, "Wrong number of channels in: %s\n", inWaveFileName);
        return 1;
    }

    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;

    TubeMonsterEngine engine;
    engine.setSampleRate(SAMPLE_RATE);

    ScaledWaveFileWriter outwave;
    const char *outWaveFileName = "test/tubemonster.wav";
    if (!outwave.Open(outWaveFileName, SAMPLE_RATE, CHANNELS))
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", outWaveFileName);
        return 1;
    }

    float inSample[CHANNELS];
    float outSample[CHANNELS];
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        // FIXFIXFIX - Update engine parameters.
        // FIXFIXFIX - Supply stereo input.
        size_t nread = inwave.Read(inSample, CHANNELS);
        if ((int)nread < CHANNELS)
            break;
        engine.process(outSample[0], outSample[1], inSample[0], inSample[1]);
        outwave.WriteSamples(outSample, CHANNELS);
    }

    return 0;
}
