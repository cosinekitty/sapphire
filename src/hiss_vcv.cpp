#include "sapphire_vcvrack.hpp"
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
            CHANNEL_COUNT_PARAM,
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


        struct HissModule : SapphireModule
        {
            RandomVectorGenerator rand;
            ChannelCountQuantity *channelCountQuantity{};

            HissModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                , rand(rack::random::u64())
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                for (int i = 0; i < NumOutputs; ++i)
                    configOutput(NOISE_OUTPUTS + i, std::string("Noise ") + std::to_string(i + 1));

                channelCountQuantity = configChannelCount(CHANNEL_COUNT_PARAM, DefaultDimensions);

                initialize();
            }

            int dimensions() const
            {
                return channelCountQuantity->getDesiredChannelCount();
            }

            void initialize()
            {
                rand.initialize();
                channelCountQuantity->initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                json_object_set_new(root, "channels", json_integer(dimensions()));
                jsonSaveSeed(root, "seed", rand.getSeed());
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);

                if (json_t* jChannels = json_object_get(root, "channels"); json_is_integer(jChannels))
                    if (json_int_t channels = json_integer_value(jChannels); channels >= 1 && channels <= 16)
                        channelCountQuantity->value = static_cast<float>(channels);

                uint64_t seed = jsonLoadOrGenerateSeed(root, "seed");
                rand.setSeed(seed);
            }

            void process(const ProcessArgs& args) override
            {
                const int dim = dimensions();
                currentChannelCount = dim;
                for (int i = 0; i < NumOutputs; ++i)
                {
                    Output& op = outputs.at(NOISE_OUTPUTS + i);
                    // Reduce CPU overhead by generating noise to connected output ports only.
                    if (op.isConnected())
                    {
                        op.setChannels(dim);
                        for (int d = 0; d < dim; ++d)
                            op.setVoltage(rand.next(), d);
                    }
                }
            }
        };

        struct HissWidget : SapphireWidget
        {
            HissModule *hissModule;

            explicit HissWidget(HissModule* module)
                : SapphireWidget("hiss", asset::plugin(pluginInstance, "res/hiss.svg"))
                , hissModule(module)
            {
                setModule(module);

                for (int i = 0; i < NumOutputs; ++i)
                    addSapphireOutput(NOISE_OUTPUTS + i, std::string("random_output_") + std::to_string(i + 1));

                addSapphireChannelDisplay("channel_display");
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
                if (hissModule)
                {
                    menu->addChild(new ChannelCountSlider(hissModule->channelCountQuantity));
                }
            }
        };
    }
}


Model* modelSapphireHiss = createSapphireModel<Sapphire::Hiss::HissModule, Sapphire::Hiss::HissWidget>(
    "Hiss",
    Sapphire::ExpanderRole::None
);
