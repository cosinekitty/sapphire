#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "vina_rk4.hpp"

namespace Sapphire      // Indranīla (इन्द्रनील)
{
    namespace Vina      // (Sanskrit: वीणा IAST: vīṇā)
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            VOCT_INPUT,
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

        struct ChannelInfo
        {
            VinaEngine engine;
            GateTriggerReceiver gateReceiver;

            void initialize()
            {
                engine.initialize();
                engine.setPreSettledState();
                gateReceiver.initialize();
            }
        };

        struct VinaModule : SapphireModule
        {
            int numActiveChannels = 0;
            ChannelInfo channelInfo[PORT_MAX_CHANNELS];

            explicit VinaModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(GATE_INPUT, "Gate");
                configInput(VOCT_INPUT, "V/OCT");
                configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                initialize();
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
            }

            void process(const ProcessArgs& args) override
            {
                numActiveChannels = numOutputChannels(INPUTS_LEN, 1);
                outputs[AUDIO_LEFT_OUTPUT].setChannels(numActiveChannels);
                outputs[AUDIO_RIGHT_OUTPUT].setChannels(numActiveChannels);
                float gateVoltage = 0;
                float voctVoltage = 0;
                for (int c = 0; c < numActiveChannels; ++c)
                {
                    ChannelInfo& q = channelInfo[c];

                    nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                    nextChannelInputVoltage(voctVoltage, VOCT_INPUT, c);

                    q.gateReceiver.update(gateVoltage);
                    if (q.gateReceiver.isTriggerActive())
                        q.engine.pluck();

                    q.engine.setPitch(voctVoltage);
                    auto frame = q.engine.update(args.sampleRate, q.gateReceiver.isGateActive());
                    outputs[AUDIO_LEFT_OUTPUT].setVoltage (frame.sample[0], c);
                    outputs[AUDIO_RIGHT_OUTPUT].setVoltage(frame.sample[1], c);
                }
            }
        };

        struct VinaWidget : SapphireWidget
        {
            explicit VinaWidget(VinaModule *module)
                : SapphireWidget("vina", asset::plugin(pluginInstance, "res/vina.svg"))
            {
                setModule(module);
                addSapphireInput(GATE_INPUT, "gate_input");
                addSapphireInput(VOCT_INPUT, "voct_input");
                addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
            }
        };
    }
}

Model* modelSapphireVina = createSapphireModel<Sapphire::Vina::VinaModule, Sapphire::Vina::VinaWidget>(
    "Vina",
    Sapphire::ExpanderRole::None
);
