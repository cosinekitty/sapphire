#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "vina_rk4.hpp"

namespace Sapphire      // Indranīla (इन्द्रनील)
{
    namespace Vina      // (Sanskrit: वीणा IAST: vīṇā)
    {
        enum ParamId
        {
            STIFFNESS_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            VOCT_INPUT,
            AUDIO_LEFT_INPUT,
            AUDIO_RIGHT_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_LEFT_OUTPUT,
            AUDIO_RIGHT_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct StiffnessSlider : SapphireSlider
        {
            explicit StiffnessSlider(SapphireQuantity* _quantity)
                : SapphireSlider(_quantity, "spring stiffness")
                {}
        };


        struct ChannelInfo
        {
            VinaEngine engine;
            GateTriggerReceiver gateReceiver;

            void initialize()
            {
                engine.initialize();
                gateReceiver.initialize();
            }
        };

        struct VinaModule : SapphireModule
        {
            int numActiveChannels = 0;
            ChannelInfo channelInfo[PORT_MAX_CHANNELS];
            SapphireQuantity* stiffnessQuantity{};

            explicit VinaModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(GATE_INPUT, "Gate");
                configInput(VOCT_INPUT, "V/OCT");
                configInput(AUDIO_LEFT_INPUT, "Left audio");
                configInput(AUDIO_RIGHT_INPUT, "Right audio");
                configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                addStiffnessSlider();
                initialize();
            }

            void addStiffnessSlider()
            {
                if (stiffnessQuantity == nullptr)
                {
                    stiffnessQuantity = configParam<SapphireQuantity>(
                        STIFFNESS_PARAM,
                        40.0, 160.0, 89.0,
                        "Stiffness"
                    );
                }
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void initialize()
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    channelInfo[c].initialize();

                stiffnessQuantity->value = stiffnessQuantity->defaultValue;
                stiffnessQuantity->changed = true;
            }

            void process(const ProcessArgs& args) override
            {
                numActiveChannels = numOutputChannels(INPUTS_LEN, 1);
                outputs[AUDIO_LEFT_OUTPUT ].setChannels(numActiveChannels);
                outputs[AUDIO_RIGHT_OUTPUT].setChannels(numActiveChannels);
                float gateVoltage = 0;
                float voctVoltage = 0;
                float leftAudioIn = 0;
                float rightAudioIn = 0;

                for (int c = 0; c < numActiveChannels; ++c)
                {
                    ChannelInfo& q = channelInfo[c];

                    nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                    nextChannelInputVoltage(voctVoltage, VOCT_INPUT, c);
                    nextChannelInputVoltage(leftAudioIn, AUDIO_LEFT_INPUT, c);
                    nextChannelInputVoltage(rightAudioIn, AUDIO_RIGHT_INPUT, c);

                    bool gate = q.gateReceiver.updateGate(gateVoltage);
                    q.engine.setPitch(voctVoltage);
                    q.engine.sim.deriv.k = stiffnessQuantity->value / 1.0e-3f;       // stiffness/mass
                    auto frame = q.engine.update(args.sampleRate, gate, leftAudioIn, rightAudioIn);
                    outputs[AUDIO_LEFT_OUTPUT ].setVoltage(frame.sample[0], c);
                    outputs[AUDIO_RIGHT_OUTPUT].setVoltage(frame.sample[1], c);
                }

                stiffnessQuantity->changed = false;
            }
        };

        struct VinaWidget : SapphireWidget
        {
            VinaModule* vinaModule{};

            explicit VinaWidget(VinaModule *module)
                : SapphireWidget("vina", asset::plugin(pluginInstance, "res/vina.svg"))
                , vinaModule(module)
            {
                setModule(module);
                addSapphireInput(GATE_INPUT, "gate_input");
                addSapphireInput(VOCT_INPUT, "voct_input");
                addSapphireInput(AUDIO_LEFT_INPUT, "audio_left_input");
                addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");
                addSapphireOutput(AUDIO_LEFT_OUTPUT,  "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
            }

            void appendContextMenu(Menu* menu) override
            {
                if (vinaModule)
                {
                    menu->addChild(new MenuSeparator);
                    menu->addChild(new StiffnessSlider(vinaModule->stiffnessQuantity));
                }
            }
        };
    }
}

Model* modelSapphireVina = createSapphireModel<Sapphire::Vina::VinaModule, Sapphire::Vina::VinaWidget>(
    "Vina",
    Sapphire::ExpanderRole::None
);
