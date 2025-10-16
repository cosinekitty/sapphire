#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "vina_engine.hpp"

namespace Sapphire      // Indranīla (इन्द्रनील)
{
    namespace Vina      // (Sanskrit: वीणा IAST: vīṇā)
    {
        constexpr int MinOctave = -3;
        constexpr int MaxOctave = +3;
        constexpr float CenterFreqHz = 261.6255653005986;   // C4 = 440/(2**0.75) Hz

        enum ParamId
        {
            STIFFNESS_PARAM,
            FREQ_PARAM,
            FREQ_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            VOCT_INPUT,
            FREQ_CV_INPUT,
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
            PortLabelMode inputPortMode = PortLabelMode::Stereo;
            PortLabelMode outputPortMode = PortLabelMode::Stereo;

            explicit VinaModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configParam(FREQ_PARAM, MinOctave, MaxOctave, 0, "Frequency", " Hz", 2, CenterFreqHz);
                configAtten(FREQ_ATTEN, "Frequency");
                configInput(FREQ_CV_INPUT, "Frequency CV");
                configInput(GATE_INPUT, "Gate");
                configInput(VOCT_INPUT, "V/OCT");
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
                        Vina::defaultStiffness - 1,
                        Vina::defaultStiffness + 1,
                        Vina::defaultStiffness,
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

                for (int c = 0; c < numActiveChannels; ++c)
                {
                    ChannelInfo& q = channelInfo[c];

                    nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                    nextChannelInputVoltage(voctVoltage, VOCT_INPUT, c);

                    bool gate = q.gateReceiver.updateGate(gateVoltage);
                    q.engine.setPitch(voctVoltage);
                    q.engine.setStiffness(stiffnessQuantity->value);
                    auto frame = q.engine.update(args.sampleRate, gate);
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
                addSapphireOutput(AUDIO_LEFT_OUTPUT,  "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                addSapphireFlatControlGroup("freq", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
            }

            void appendContextMenu(Menu* menu) override
            {
                if (vinaModule)
                {
                    menu->addChild(new MenuSeparator);
                    menu->addChild(new StiffnessSlider(vinaModule->stiffnessQuantity));
                }
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);
                if (vinaModule)
                {
                    drawAudioPortLabels(args.vg, vinaModule->inputPortMode,  "left_input_label",  "right_input_label" );
                    drawAudioPortLabels(args.vg, vinaModule->outputPortMode, "left_output_label", "right_output_label");
                }
            }
        };
    }
}

Model* modelSapphireVina = createSapphireModel<Sapphire::Vina::VinaModule, Sapphire::Vina::VinaWidget>(
    "Vina",
    Sapphire::ExpanderRole::None
);
