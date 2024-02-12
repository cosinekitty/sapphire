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

    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 2;
    const int DURATION_SECONDS = 10;
    const int DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS;
    const int SETTLE_SECONDS = 1;
    const int SETTLE_SAMPLES = SAMPLE_RATE * SETTLE_SECONDS;

    const size_t NUM_PARTICLES = 5;
    NucleusEngine engine{NUM_PARTICLES};
    SetMinimumEnergy(engine);
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

    // Apply a pulse from 500 ms to 510 ms.
    const int pulseStartSample  = SampleFromSeconds(0.500, SAMPLE_RATE);
    const int pulseFinishSample = SampleFromSeconds(1.000, SAMPLE_RATE);

    float sample[CHANNELS];
    for (int s = 0; s < DURATION_SAMPLES; ++s)
    {
        Particle& input = engine.particle(0);
        input.pos[0] = (s >= pulseStartSample && s <= pulseFinishSample) ? 10 * inputDrive : 0;
        input.pos[1] = 0;
        input.pos[2] = 0;
        input.pos[3] = 0;
        input.vel = PhysicsVector::zero();
        engine.update(speed/SAMPLE_RATE, halflife, SAMPLE_RATE, 1.0f);
        if (crashChecker.check(engine))
        {
            fprintf(stderr, "ERROR: Nucleus engine produced non-finite output at sample %d (rendering).\n", s);
            return 1;
        }
        sample[0] = engine.output(1, 0);    // BX
        sample[1] = engine.output(1, 1);    // BY
        wave.WriteSamples(sample, CHANNELS);
    }

    return 0;
}

/*

{
  "plugin": "CosineKitty-Sapphire",
  "model": "Nucleus",
  "version": "2.4.1",
  "params": [
    {
      "value": 5.0,
      "id": 0
    },
    {
      "value": 0.92349445819854736,
      "id": 1
    },
    {
      "value": 0.20915652811527252,
      "id": 2
    },
    {
      "value": 1.0481926202774048,
      "id": 3
    },
    {
      "value": 1.3204823732376099,
      "id": 4
    },
    {
      "value": 0.0,
      "id": 5
    },
    {
      "value": 0.0,
      "id": 6
    },
    {
      "value": 0.0,
      "id": 7
    },
    {
      "value": 0.0,
      "id": 8
    },
    {
      "value": 0.0,
      "id": 9
    },
    {
      "value": 1.0,
      "id": 10
    },
    {
      "value": 4.0,
      "id": 11
    }
  ],
  "data": {
    "limiterWarningLight": true,
    "agcLevel": 4.0,
    "tricorderOutputIndex": 1
  }
}
*/