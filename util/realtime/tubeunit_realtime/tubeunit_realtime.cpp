#include <cstdio>
#include <mutex>
#include <queue>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "tubeunit_engine.hpp"

enum class CommandKind
{
    SetAirflow,
    SetRootFrequency,
    SetSpringConstant,
};

struct RenderCommand
{
    CommandKind kind;
    float value;

    RenderCommand(CommandKind _kind, float _value)
        : kind(_kind)
        , value(_value)
        {}
};

struct RenderContext
{
    Sapphire::TubeUnitEngine engine;
    std::mutex lock;
    std::queue<RenderCommand> commandQueue;

    void ProcessCommands()
    {
        std::lock_guard<std::mutex> guard(lock);

        while (!commandQueue.empty())
        {
            RenderCommand command = commandQueue.front();
            commandQueue.pop();
            switch (command.kind)
            {
            case CommandKind::SetAirflow:
                engine.setAirflow(command.value);
                break;

            case CommandKind::SetRootFrequency:
                engine.setRootFrequency(command.value);
                break;

            case CommandKind::SetSpringConstant:
                engine.setSpringConstant(command.value);
                break;
            }
        }
    }

    void SendCommand(CommandKind _kind, float _value)
    {
        std::lock_guard<std::mutex> guard(lock);
        commandQueue.push(RenderCommand{ _kind, _value });
    }

    void ReadParameters()
    {
        float airflow;
        float rootFrequency;
        float springConstant;
        {
            std::lock_guard<std::mutex> guard(lock);
            airflow = engine.getAirFlow();
            rootFrequency = engine.getRootFrequency();
            springConstant = engine.getSpringConstant();
        }
        printf("airflow = %f\n", airflow);
        printf("root frequency = %f\n", rootFrequency);
        printf("spring constant = %f\n", springConstant);
    }

    void Initialize()
    {
        std::lock_guard<std::mutex> guard(lock);
        engine.initialize();
    }
};


static void AudioDataCallback(
    ma_device* device,
    void* output,
    const void *input,
    ma_uint32 frameCount)
{
    using namespace std;
    using namespace Sapphire;

    float* data = static_cast<float*>(output);
    RenderContext& context = *static_cast<RenderContext*>(device->pUserData);

    context.ProcessCommands();

    for (ma_uint32 frame = 0; frame < frameCount; ++frame)
    {
        context.engine.process(data[2*frame], data[2*frame + 1]);
    }
}


std::vector<std::string> Tokenize(const char* line)
{
    using namespace std;

    vector<string> tokens;
    bool inspace = true;
    for (int i = 0; line[i] != '\0' && line[i] != '\n' && line[i] != '\r' && line[i] != '#'; ++i)
    {
        if (line[i] == ' ' || line[i] == '\t')
        {
            inspace = true;
        }
        else
        {
            if (inspace)
            {
                inspace = false;
                tokens.push_back(string(1, line[i]));
            }
            else
                tokens.back() += line[i];
        }
    }
    return tokens;
}


void PrintHelp()
{
    printf(
        "\n"
        "Commands:\n"
        "\n"
        "    h   = Print this help text.\n"
        "    q   = Quit.\n"
        "    r   = Read current parameters.\n"
        "    i   = Initialize.\n"
        "    a x = Set airflow to x [-1, +1].\n"
        "    f x = Set root frequency to x [1, 10000].\n"
        "    k x = Set spring constant to x [1e-6, 1e+6].\n"
        "\n"
    );
}


int main(int argc, const char* argv[])
{
    using namespace std;
    using namespace Sapphire;

    const int SAMPLE_RATE = 44100;

    RenderContext context;

    context.engine.setSampleRate(SAMPLE_RATE);
    context.engine.setAirflow(1.0);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = AudioDataCallback;
    config.pUserData = &context;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
        fprintf(stderr, "ERROR: Cannot initialize miniaudio device.\n");
        return 1;
    }

    ma_device_start(&device);

    PrintHelp();

    char line[256];
    while (fgets(line, sizeof(line), stdin))
    {
        vector<string> tokens = Tokenize(line);

        if (tokens.size() > 0)
        {
            if (tokens[0] == "q")
                break;

            if (tokens[0] == "h")
            {
                PrintHelp();
                continue;
            }

            if (tokens[0] == "a" && tokens.size() == 2)
            {
                context.SendCommand(CommandKind::SetAirflow, atof(tokens[1].c_str()));
                continue;
            }

            if (tokens[0] == "f" && tokens.size() == 2)
            {
                context.SendCommand(CommandKind::SetRootFrequency, atof(tokens[1].c_str()));
                continue;
            }

            if (tokens[0] == "k" && tokens.size() == 2)
            {
                context.SendCommand(CommandKind::SetSpringConstant, atof(tokens[1].c_str()));
                continue;
            }

            if (tokens[0] == "r")
            {
                context.ReadParameters();
                continue;
            }

            if (tokens[0] == "i")
            {
                context.Initialize();
                continue;
            }

            printf("??? Invalid command.\n");
        }
    }

    ma_device_uninit(&device);

    return 0;
}

