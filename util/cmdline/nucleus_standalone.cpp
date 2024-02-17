#include <cstdio>
#include <cmath>
#include "nucleus_engine.hpp"
#include "nucleus_init.hpp"
#include "nucleus_reset.hpp"
#include "wavefile.hpp"

inline int SampleFromSeconds(float seconds, float sampleRate)
{
    return static_cast<int>(std::round(seconds * sampleRate));
}

int main()
{
    using namespace std;
    using namespace Sapphire;

    const int SAMPLE_RATE = 44100;      // CD sampling rate
    const int CHANNELS = 2;             // stereo output
    const int DURATION_SECONDS = 10;
    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;
    const int SETTLE_SECONDS = 1;       // how many seconds to allow initial settling of the Nucleus engine
    const int SETTLE_SAMPLES = SAMPLE_RATE * SETTLE_SECONDS;

    NucleusEngine engine{5};
    Nucleus::SetMinimumEnergy(engine);
    const float speed = pow(2.0f, 5.0f - 1);
    engine.setMagneticCoupling(0.21 * 0.09);
    engine.setDcRejectEnabled(true);
    const float halflife = pow(10.0f, 5*0.85f - 3);
    const float inputDrive = 0.015f;

    ScaledWaveFileWriter wave;
    const char *filename = "test/nucleus.wav";
    if (!wave.Open(filename, SAMPLE_RATE, CHANNELS))
    {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", filename);
        return 1;
    }

    Sapphire::Nucleus::CrashChecker crashChecker;

    // Keep running the engine until it gets past linear transition from
    // filter-disabled to filter-enabled.
    for (int s = 0; s < SETTLE_SAMPLES; ++s)
    {
        engine.particle(0).pos = PhysicsVector::zero();
        engine.particle(0).vel = PhysicsVector::zero();
        engine.update(speed/SAMPLE_RATE, halflife, SAMPLE_RATE, 1.0f);
        if (crashChecker.check(engine))
        {
            fprintf(stderr, "ERROR: Nucleus engine produced non-finite output at sample %d (settling).\n", s);
            return 1;
        }
    }

    // Apply a square wave pulse as input over the following range of samples.
    const int pulseStartSample  = SampleFromSeconds(0.500, SAMPLE_RATE);
    const int pulseFinishSample = SampleFromSeconds(1.000, SAMPLE_RATE);

    float sample[CHANNELS];
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        // Set the position of particle A as input.
        Particle& input = engine.particle(0);
        input.pos[0] = (s >= pulseStartSample && s <= pulseFinishSample) ? 10 * inputDrive : 0;
        input.pos[1] = 0;
        input.pos[2] = 0;
        input.pos[3] = 0;
        // Also make sure particle A's velocity is always zero.
        input.vel = PhysicsVector::zero();

        // Run the simulation for one sample.
        engine.update(speed/SAMPLE_RATE, halflife, SAMPLE_RATE, 1.0f);

        // Look for any non-finite numbers in the simulation.
        if (crashChecker.check(engine))
        {
            fprintf(stderr, "ERROR: Nucleus engine produced non-finite output at sample %d (rendering).\n", s);
            return 1;
        }

        // Use (BX, BY) outputs as stereo (left, right) for the output WAV file.
        sample[0] = engine.output(1, 0);    // BX
        sample[1] = engine.output(1, 1);    // BY
        wave.WriteSamples(sample, CHANNELS);
    }

    return 0;
}
