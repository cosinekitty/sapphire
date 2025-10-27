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
            OBSOLETE_1_PARAM,
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


        struct ChannelInfo
        {
            VinaWire wire;
            GateTriggerReceiver gateReceiver;

            void initialize()
            {
                wire.initialize();
                gateReceiver.initialize();
            }
        };

        struct VinaModule : SapphireModule
        {
            int numActiveChannels = 0;
            ChannelInfo channelInfo[PORT_MAX_CHANNELS];
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

            bool isReverbEnabled() const
            {
                return channelInfo[0].wire.isReverbEnabled;
            }

            void setReverbEnabled(bool enable)
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    channelInfo[c].wire.isReverbEnabled = enable;
            }

            bool isStandbyEnabled() const
            {
                return channelInfo[0].wire.isStandbyEnabled;
            }

            void setStandbyEnabled(bool enable)
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    channelInfo[c].wire.setStandbyEnabled(enable);
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                jsonSetBool(root, "isReverbEnabled", isReverbEnabled());
                jsonSetBool(root, "isStandbyEnabled", isStandbyEnabled());
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                loadReverbEnabledFlag(root);
                loadStandbyEnabledFlag(root);
            }

            void loadReverbEnabledFlag(json_t* root)
            {
                bool e = isReverbEnabled();
                jsonLoadBool(root, "isReverbEnabled", e);
                setReverbEnabled(e);
            }

            void loadStandbyEnabledFlag(json_t* root)
            {
                bool e = isStandbyEnabled();
                jsonLoadBool(root, "isStandbyEnabled", e);
                setStandbyEnabled(e);
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
                    nextChannelInputVoltage(cvDecay, DECAY_CV_INPUT, c);
                    nextChannelInputVoltage(cvRelease, RELEASE_CV_INPUT, c);
                    nextChannelInputVoltage(cvPan, PAN_CV_INPUT, c);

                    bool gate = q.gateReceiver.updateGate(gateVoltage);

                    float raw = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, cvFreq, MinOctave, MaxOctave);
                    float oct = cvGetVoltPerOctave(OCT_PARAM,  OCT_ATTEN,  cvOct,  MinOctave, MaxOctave);
                    float level = cvGetControlValue(LEVEL_PARAM, LEVEL_ATTEN, cvLevel, 0, 2);
                    float pan = cvGetControlValue(PAN_PARAM, PAN_ATTEN, cvPan, -1, +1);
                    float decay = cvGetControlValue(DECAY_PARAM, DECAY_ATTEN, cvDecay, 0, 1);
                    float release = cvGetControlValue(RELEASE_PARAM, RELEASE_ATTEN, cvRelease, 0, 1);
                    float freq = raw + std::round(oct) + voctVoltage;
                    if (freq < MinOctave-1 || freq > MaxOctave+1)
                        gate = false;       // ignore notes outside the instrument's range
                    else
                        q.wire.setPitch(freq);

                    q.wire.setLevel(level);
                    q.wire.setDecay(decay);
                    q.wire.setRelease(release);
                    q.wire.setPan(pan);
                    auto frame = q.wire.update(args.sampleRate, gate);
                    outputs[AUDIO_LEFT_OUTPUT ].setVoltage(frame.sample[0], c);
                    outputs[AUDIO_RIGHT_OUTPUT].setVoltage(frame.sample[1], c);
                }
            }
        };


        struct ToggleReverbAction : history::Action
        {
            const int64_t moduleId;

            explicit ToggleReverbAction(int64_t _moduleId)
                : moduleId(_moduleId)
            {
                name = "toggle internal reverb";
            }

            void toggle();
            void undo() override { toggle(); }
            void redo() override { toggle(); }
        };


        struct ToggleStandbyAction : history::Action
        {
            const int64_t moduleId;

            explicit ToggleStandbyAction(int64_t _moduleId)
                : moduleId(_moduleId)
            {
                name = "toggle standby";
            }

            void toggle();
            void undo() override { toggle(); }
            void redo() override { toggle(); }
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
                SapphireWidget::appendContextMenu(menu);
                if (vinaModule)
                {
                    menu->addChild(new MenuSeparator);

                    menu->addChild(createBoolMenuItem(
                        "Enable internal reverb",
                        "",
                        [=]() { return vinaModule->isReverbEnabled(); },
                        [=](bool state)
                        {
                            if (state != vinaModule->isReverbEnabled())
                                InvokeAction(new ToggleReverbAction(vinaModule->id));
                        }
                    ));

                    menu->addChild(createBoolMenuItem(
                        "Enable standby",
                        "",
                        [=]() { return vinaModule->isStandbyEnabled(); },
                        [=](bool state)
                        {
                            if (state != vinaModule->isStandbyEnabled())
                                InvokeAction(new ToggleStandbyAction(vinaModule->id));
                        }
                    ));
                }
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);
                PortLabelMode outputPortMode = vinaModule ? vinaModule->outputPortMode : PortLabelMode::Stereo;
                drawAudioPortLabels(args.vg, outputPortMode, "left_output_label", "right_output_label");
            }
        };


        void ToggleReverbAction::toggle()
        {
            if (VinaWidget* w = FindSapphireWidget<VinaWidget>(moduleId))
                if (VinaModule* m = w->vinaModule)
                    m->setReverbEnabled(!m->isReverbEnabled());
        }


        void ToggleStandbyAction::toggle()
        {
            if (VinaWidget* w = FindSapphireWidget<VinaWidget>(moduleId))
                if (VinaModule* m = w->vinaModule)
                    m->setStandbyEnabled(!m->isStandbyEnabled());
        }
    }
}


Model* modelSapphireVina = createSapphireModel<Sapphire::Vina::VinaModule, Sapphire::Vina::VinaWidget>(
    "Vina",
    Sapphire::ExpanderRole::None
);
