#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_random.hpp"

// Sapphire Hiss for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Hiss
    {
        const int DefaultDimensions = 3;
        const int NumOutputs = 10;

        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            INPUTS_LEN
        };

        enum OutputId
        {
            ENUMS(NOISE_OUTPUTS, NumOutputs),
            OUTPUTS_LEN
        };

        enum LightId
        {
            AUDIO_MODE_BUTTON_LIGHT,
            LIGHTS_LEN
        };


        struct HissModule : Module
        {
            int dimensions = DefaultDimensions;     // 1..16 : how many dimensions (channels) each output port provides
            RandomVectorGenerator rand;

            HissModule()
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                for (int i = 0; i < NumOutputs; ++i)
                    configOutput(NOISE_OUTPUTS + i, std::string("Noise ") + std::to_string(i + 1));
                initialize();
            }

            void initialize()
            {
                dimensions = DefaultDimensions;
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t* root = json_object();
                json_object_set_new(root, "channels", json_integer(dimensions));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                json_t* channels = json_object_get(root, "channels");
                if (json_is_integer(channels))
                {
                    json_int_t n = json_integer_value(channels);
                    if (n >= 1 && n <= 16)
                        dimensions = static_cast<int>(n);
                }
            }

            void process(const ProcessArgs& args) override
            {
                for (int i = 0; i < NumOutputs; ++i)
                {
                    Output& op = outputs[NOISE_OUTPUTS + i];
                    op.setChannels(dimensions);
                    for (int d = 0; d < dimensions; ++d)
                        op.setVoltage(rand.next(), d);
                }
            }
        };


        struct HissWidget : SapphireReloadableModuleWidget
        {
            explicit HissWidget(HissModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/hiss.svg"))
            {
                setModule(module);

                for (int i = 0; i < NumOutputs; ++i)
                    addSapphireOutput(NOISE_OUTPUTS + i, std::string("random_output_") + std::to_string(i + 1));

                reloadPanel();
            }
        };
    }
}


Model* modelHiss = createSapphireModel<Sapphire::Hiss::HissModule, Sapphire::Hiss::HissWidget>(
    "Hiss",
    Sapphire::VectorRole::None
);
