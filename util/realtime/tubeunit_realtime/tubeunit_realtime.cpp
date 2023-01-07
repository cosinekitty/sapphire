#include <cstdio>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "tubeunit_engine.hpp"


static void AudioDataCallback(
    ma_device* device,
    void* output,
    const void *input,
    ma_uint32 frameCount)
{
    using namespace Sapphire;

    float* data = static_cast<float*>(output);
    TubeUnitEngine& engine = *static_cast<TubeUnitEngine*>(device->pUserData);

    for (ma_uint32 frame = 0; frame < frameCount; ++frame)
    {
        engine.process(data[2*frame], data[2*frame + 1]);
    }
}


int main(int argc, const char* argv[])
{
    using namespace std;
    using namespace Sapphire;

    const int SAMPLE_RATE = 44100;

    TubeUnitEngine engine;
    engine.setSampleRate(SAMPLE_RATE);
    engine.setAirflow(1.0);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = AudioDataCallback;
    config.pUserData = &engine;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
        fprintf(stderr, "ERROR: Cannot initialize miniaudio device.\n");
        return 1;
    }

    ma_device_start(&device);

    char line[256];
    while (fgets(line, sizeof(line), stdin))
    {
        if (line[0] == 'q')
            break;
    }

    ma_device_uninit(&device);

    return 0;
}

