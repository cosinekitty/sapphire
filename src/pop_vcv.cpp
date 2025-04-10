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
            SYNC_TRIGGER_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            PULSE_TRIGGER_OUTPUT,

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
            bool isSyncPending = false;
            GateTriggerReceiver syncReceiver[PORT_MAX_CHANNELS];
            ChannelCountQuantity *channelCountQuantity{};
            bool prevTriggerOnReset = false;
            bool sendTriggerOnReset = false;

            PopModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                channelCountQuantity = configChannelCount(CHANNEL_COUNT_PARAM, 1);

                configOutput(PULSE_TRIGGER_OUTPUT, "Pulse trigger");

                configParam(SPEED_PARAM, MIN_POP_SPEED, MAX_POP_SPEED, DEFAULT_POP_SPEED, "Speed");
                configParam(CHAOS_PARAM, MIN_POP_CHAOS, MAX_POP_CHAOS, DEFAULT_POP_CHAOS, "Chaos");

                configParam(SPEED_ATTEN, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(CHAOS_ATTEN, -1, 1, 0, "Chaos attenuverter", "%", 0, 100);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(CHAOS_CV_INPUT, "Chaos CV");
                configInput(SYNC_TRIGGER_INPUT, "Sync trigger");

                initialize();
            }

            void initialize()
            {
                isSyncPending = false;
                channelCountQuantity->initialize();

                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                {
                    engine[c].sendTriggerOnReset = sendTriggerOnReset;
                    engine[c].initialize();
                    engine[c].setRandomSeed(c*0x100001 + 0xbeef0);
                    syncReceiver[c].initialize();
                }
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
                json_object_set_new(root, "outputMode", json_integer(getOutputMode()));
                json_object_set_new(root, "triggerOnReset", json_boolean(sendTriggerOnReset));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                json_t* channels = json_object_get(root, "channels");
                if (json_is_integer(channels))
                {
                    json_int_t n = json_integer_value(channels);
                    if (n >= 1 && n <= PORT_MAX_CHANNELS)
                        channelCountQuantity->value = static_cast<float>(n);
                }

                json_t* outputMode = json_object_get(root, "outputMode");
                if (json_is_integer(outputMode))
                {
                    size_t index = json_integer_value(outputMode);
                    setOutputMode(index);
                }

                json_t* trig = json_object_get(root, "triggerOnReset");
                sendTriggerOnReset = json_is_true(trig);
            }

            void process(const ProcessArgs& args) override
            {
                if (prevTriggerOnReset != sendTriggerOnReset)
                {
                    prevTriggerOnReset = sendTriggerOnReset;
                    for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                        engine[c].sendTriggerOnReset = prevTriggerOnReset;
                }

                const int nc = desiredChannelCount();
                currentChannelCount = nc;       // keep channel display panel updated
                outputs.at(PULSE_TRIGGER_OUTPUT).setChannels(nc);
                float cvSpeed = 0;
                float cvChaos = 0;
                float vSync = 0;
                for (int c = 0; c < nc; ++c)
                {
                    nextChannelInputVoltage(cvSpeed, SPEED_CV_INPUT, c);
                    nextChannelInputVoltage(cvChaos, CHAOS_CV_INPUT, c);
                    nextChannelInputVoltage(vSync, SYNC_TRIGGER_INPUT, c);

                    bool isSyncTrigger = syncReceiver[c].updateTrigger(vSync);
                    float speed = cvGetVoltPerOctave(SPEED_PARAM, SPEED_ATTEN, cvSpeed, MIN_POP_SPEED, MAX_POP_SPEED);
                    float chaos = cvGetControlValue(CHAOS_PARAM, CHAOS_ATTEN, cvChaos, MIN_POP_CHAOS, MAX_POP_CHAOS);

                    if (isSyncTrigger || isSyncPending)
                        engine[c].initialize();
                    engine[c].setSpeed(speed);
                    engine[c].setChaos(chaos);

                    const float v = engine[c].process(args.sampleRate);
                    outputs.at(PULSE_TRIGGER_OUTPUT).setVoltage(v, c);
                }
                isSyncPending = false;
            }

            int desiredChannelCount() const
            {
                return channelCountQuantity->getDesiredChannelCount();
            }

            size_t getOutputMode() const
            {
                // We keep the output modes always in sync.
                // So the first channel's state is our single source of truth.
                return static_cast<size_t>(engine[0].getOutputMode());
            }

            void setOutputMode(size_t index)
            {
                OutputMode mode = static_cast<OutputMode>(index);
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    engine[c].setOutputMode(mode);
            }
        };


        struct PopWidget : SapphireWidget
        {
            PopModule* popModule{};

            explicit PopWidget(PopModule* module)
                : SapphireWidget("pop", asset::plugin(pluginInstance, "res/pop.svg"))
                , popModule(module)
            {
                setModule(module);

                addSapphireOutput(PULSE_TRIGGER_OUTPUT, "pulse_output");

                addKnob(SPEED_PARAM, "speed_knob");
                addKnob(CHAOS_PARAM, "chaos_knob");

                addSapphireAttenuverter(SPEED_ATTEN, "speed_atten");
                addSapphireAttenuverter(CHAOS_ATTEN, "chaos_atten");

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(CHAOS_CV_INPUT, "chaos_cv");
                addSapphireInput(SYNC_TRIGGER_INPUT, "sync_input");

                addSapphireChannelDisplay("channel_display");
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
                if (popModule != nullptr)
                {
                    addManualSyncMenuItem(menu);
                    addOutputModeMenuItems(menu);
                    menu->addChild(createBoolPtrMenuItem<bool>("Send trigger on every reset", "", &popModule->sendTriggerOnReset));
                    menu->addChild(new ChannelCountSlider(popModule->channelCountQuantity));
                }
            }

            void addManualSyncMenuItem(Menu* menu)
            {
                menu->addChild(createMenuItem(
                    "Sync polyphonic channels",
                    "",
                    [=]{ popModule->isSyncPending = true; }
                ));
            }

            void addOutputModeMenuItems(Menu* menu)
            {
                std::vector<std::string> labels {
                    "Triggers",
                    "Gates"
                };

                menu->addChild(createIndexSubmenuItem(
                    "Output pulse mode",
                    labels,
                    [=]() { return popModule->getOutputMode(); },
                    [=](size_t mode) { popModule->setOutputMode(mode); }
                ));
            }
        };
    }
}


Model* modelSapphirePop = createSapphireModel<Sapphire::Pop::PopModule, Sapphire::Pop::PopWidget>(
    "Pop",
    Sapphire::ExpanderRole::None
);
