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
            OCT_PARAM,
            OCT_ATTEN,
            LEVEL_PARAM,
            LEVEL_ATTEN,
            DECAY_PARAM,
            DECAY_ATTEN,
            RELEASE_PARAM,
            RELEASE_ATTEN,
            PAN_PARAM,
            PAN_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            VOCT_INPUT,
            FREQ_CV_INPUT,
            OCT_CV_INPUT,
            LEVEL_CV_INPUT,
            DECAY_CV_INPUT,
            RELEASE_CV_INPUT,
            PAN_CV_INPUT,
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
            PortLabelMode inputPortMode  = PortLabelMode::Stereo;
            PortLabelMode outputPortMode = PortLabelMode::Stereo;

            explicit VinaModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configParam(FREQ_PARAM, MinOctave, MaxOctave, 0, "Frequency", " Hz", 2, CenterFreqHz);
                configAtten(FREQ_ATTEN, "Frequency");
                configInput(FREQ_CV_INPUT, "Frequency CV");

                if (ParamQuantity* octParam = configParam(OCT_PARAM, MinOctave, MaxOctave, 0, "Octave"))
                    octParam->snapEnabled = true;
                configAtten(OCT_ATTEN, "Octave");
                configInput(OCT_CV_INPUT, "Octave CV");

                configParam(LEVEL_PARAM, 0, 2, 1, "Level", " dB", -10, 20*3);
                configAtten(LEVEL_ATTEN, "Level CV");
                configInput(LEVEL_CV_INPUT, "Level CV");

                configParam(PAN_PARAM, -1, +1, 0, "Pan");
                configAtten(PAN_ATTEN, "Pan CV");
                configInput(PAN_CV_INPUT, "Pan CV");

                configParam(DECAY_PARAM, 0, 1, 0.5, "Decay");
                configAtten(DECAY_ATTEN, "Decay CV");
                configInput(DECAY_CV_INPUT, "Decay CV");

                configParam(RELEASE_PARAM, 0, 1, 0.5, "Release");
                configAtten(RELEASE_ATTEN, "Release CV");
                configInput(RELEASE_CV_INPUT, "Release CV");

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
                float cvFreq = 0;
                float cvOct = 0;
                float cvLevel = 0;
                float cvDecay = 0;
                float cvRelease = 0;
                float cvPan = 0;

                for (int c = 0; c < numActiveChannels; ++c)
                {
                    ChannelInfo& q = channelInfo[c];

                    nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                    nextChannelInputVoltage(voctVoltage, VOCT_INPUT, c);
                    nextChannelInputVoltage(cvFreq, FREQ_CV_INPUT, c);
                    nextChannelInputVoltage(cvOct, OCT_CV_INPUT, c);
                    nextChannelInputVoltage(cvLevel, LEVEL_CV_INPUT, c);
                    nextChannelInputVoltage(cvPan, PAN_CV_INPUT, c);

                    bool gate = q.gateReceiver.updateGate(gateVoltage);

                    float raw  = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, cvFreq, MinOctave, MaxOctave);
                    float oct  = cvGetVoltPerOctave(OCT_PARAM,  OCT_ATTEN,  cvOct,  MinOctave, MaxOctave);
                    float level   = cvGetControlValue(LEVEL_PARAM, LEVEL_ATTEN, cvLevel, 0, 2);
                    float pan     = cvGetControlValue(PAN_PARAM, PAN_ATTEN, cvPan, -1, +1);
                    float decay   = cvGetControlValue(DECAY_PARAM, DECAY_ATTEN, cvDecay, 0, 1);
                    float release = cvGetControlValue(RELEASE_PARAM, RELEASE_ATTEN, cvRelease, 0, 1);
                    float freq = raw + std::round(oct) + voctVoltage;
                    if (freq < MinOctave-1 || freq > MaxOctave+1)
                        gate = false;       // ignore notes outside the instrument's range
                    else
                        q.engine.setPitch(freq);

                    q.engine.setStiffness(stiffnessQuantity->value);
                    q.engine.setLevel(level);
                    q.engine.setDecay(decay);
                    q.engine.setRelease(release);
                    q.engine.setPan(pan);
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
                addSapphireFlatControlGroup("freq",    FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
                addSapphireFlatControlGroup("oct",     OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT);
                addSapphireFlatControlGroup("level",   LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT);
                addSapphireFlatControlGroup("pan",     PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                addSapphireFlatControlGroup("decay",   DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                addSapphireFlatControlGroup("release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);
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
