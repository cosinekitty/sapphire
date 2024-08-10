#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "pop_engine.hpp"

namespace Sapphire
{
    namespace Pop
    {
        enum ParamId
        {
            SPEED_PARAM,
            SPEED_ATTEN,
            CHAOS_PARAM,
            CHAOS_ATTEN,
            CHANNEL_COUNT_PARAM,

            PARAMS_LEN
        };

        enum InputId
        {
            SPEED_CV_INPUT,
            CHAOS_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            TRIGGER_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct PopModule : SapphireModule
        {
            int nPolyChannels = 1;      // current number of output channels (how many of `engine` array to use)
            Engine engine[PORT_MAX_CHANNELS];
            ChannelCountQuantity *channelCountQuantity{};

            PopModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                channelCountQuantity = configChannelCount(CHANNEL_COUNT_PARAM, 1);

                configOutput(TRIGGER_OUTPUT, "Trigger");

                configParam(SPEED_PARAM, MIN_POP_SPEED, MAX_POP_SPEED, DEFAULT_POP_SPEED, "Speed");
                configParam(CHAOS_PARAM, MIN_POP_CHAOS, MAX_POP_CHAOS, DEFAULT_POP_CHAOS, "Chaos");

                configParam(SPEED_ATTEN, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(CHAOS_ATTEN, -1, 1, 0, "Chaos attenuverter", "%", 0, 100);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(CHAOS_CV_INPUT, "Chaos CV");

                initialize();
            }

            void initialize()
            {
                channelCountQuantity->initialize();

                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    engine[c].initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                json_object_set_new(root, "channels", json_integer(desiredChannelCount()));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                json_t* channels = json_object_get(root, "channels");
                if (json_is_integer(channels))
                {
                    json_int_t n = json_integer_value(channels);
                    if (n >= 1 && n <= 16)
                        channelCountQuantity->value = static_cast<float>(n);
                }
            }

            void process(const ProcessArgs& args) override
            {
                const int nc = desiredChannelCount();
                outputs[TRIGGER_OUTPUT].setChannels(nc);
                float cvSpeed = 0;
                float cvChaos = 0;
                for (int c = 0; c < nc; ++c)
                {
                    nextChannelInputVoltage(cvSpeed, SPEED_CV_INPUT, c);
                    nextChannelInputVoltage(cvChaos, CHAOS_CV_INPUT, c);
                    float speed = cvGetControlValue(SPEED_PARAM, SPEED_ATTEN, cvSpeed, MIN_POP_SPEED, MAX_POP_SPEED);
                    float chaos = cvGetControlValue(CHAOS_PARAM, CHAOS_ATTEN, cvChaos, MIN_POP_CHAOS, MAX_POP_CHAOS);
                    engine[c].setSpeed(speed);
                    engine[c].setChaos(chaos);
                    const float v = engine[c].process(args.sampleRate);
                    outputs[TRIGGER_OUTPUT].setVoltage(v, c);
                }
            }

            int desiredChannelCount() const
            {
                return channelCountQuantity->getDesiredChannelCount();
            }
        };


        struct PopWidget : SapphireReloadableModuleWidget
        {
            PopModule* popModule{};

            explicit PopWidget(PopModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/pop.svg"))
                , popModule(module)
            {
                setModule(module);

                addSapphireOutput(TRIGGER_OUTPUT, "trigger_output");

                addKnob(SPEED_PARAM, "speed_knob");
                addKnob(CHAOS_PARAM, "chaos_knob");

                addSapphireAttenuverter(SPEED_ATTEN, "speed_atten");
                addSapphireAttenuverter(CHAOS_ATTEN, "chaos_atten");

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(CHAOS_CV_INPUT, "chaos_cv");

                reloadPanel();
            }

            void appendContextMenu(Menu* menu) override
            {
                if (popModule != nullptr)
                {
                    menu->addChild(new MenuSeparator);
                    menu->addChild(new ChannelCountSlider(popModule->channelCountQuantity));
                }
            }
        };
    }
}


Model* modelSapphirePop = createSapphireModel<Sapphire::Pop::PopModule, Sapphire::Pop::PopWidget>(
    "Pop",
    Sapphire::VectorRole::None
);
