/*
    Don Cross <cosinekitty@gmail.com>
*/

#include <string>
#include <vector>
#include "env_pitch_detect.hpp"
#include "wavefile.hpp"

int main()
{
    using namespace std;
    using namespace Sapphire;

    constexpr int nchannels = 2;
    using engine_t = EnvPitchDetector<float, nchannels>;

    WaveFileReader inwave;
    const char *inWaveFileName = "input/DryGuitarForDon-16-bit.wav";
    if (!inwave.Open(inWaveFileName))
    {
        fprintf(stderr, "ERROR: Cannot open input file: %s\n", inWaveFileName);
        return 1;
    }

    const int CHANNELS = inwave.Channels();
    const int SAMPLE_RATE = inwave.SampleRate();
    printf("envpitch(%s): Sample rate = %d, channels = %d\n", inWaveFileName, SAMPLE_RATE, CHANNELS);

    if (CHANNELS != nchannels)
    {
        fprintf(stderr, "Wrong number of channels %d in: %s; expected %d.\n", CHANNELS, inWaveFileName, nchannels);
        return 1;
    }

    engine_t engine;

    float inFrame[nchannels];
    float envelope[nchannels];
    float pitch[nchannels];

    // Every tenth of a second, take a snapshot of the envelope and pitch.
    const int frameInterval = SAMPLE_RATE / 10;

    const char *outFileName = "test/envpitch.txt";
    FILE *outfile = fopen(outFileName, "wt");
    if (outfile == nullptr)
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", outFileName);
        return 1;
    }

    for (int f = 0; (int)inwave.Read(inFrame, nchannels) == nchannels; ++f)
    {
        // Make sure envelope and pitch are written to.
        for (int c = 0; c < nchannels; ++c)
            envelope[c] = pitch[c] = NAN;

        engine.process(nchannels, SAMPLE_RATE, inFrame, envelope, pitch);
        if (f % frameInterval == 0)
            fprintf(outfile, "f=%d, envelope=(%g, %g), pitch=(%g, %g)\n", f, envelope[0], envelope[1], pitch[0], pitch[1]);
    }

    fclose(outfile);
    return 0;
}
