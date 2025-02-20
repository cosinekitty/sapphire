#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "env_pitch_detect.hpp"

// Sapphire Env for VCV Rack by Don Cross <cosinekitty@gmail.com>.
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Env
    {
        enum ParamId
        {
            THRESHOLD_PARAM,
            THRESHOLD_ATTEN,
            SPEED_PARAM,
            SPEED_ATTEN,
            FREQ_PARAM,
            FREQ_ATTEN,
            RES_PARAM,
            RES_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            THRESHOLD_CV_INPUT,
            SPEED_CV_INPUT,
            FREQ_CV_INPUT,
            RES_CV_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            ENVELOPE_OUTPUT,
            PITCH_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        using detector_t = EnvPitchDetector<float, PORT_MAX_CHANNELS>;

        enum class EnvPortMode
        {
            Linear,
            GateActiveHi,
            GateActiveLo,
            LEN
        };

        struct EnvModule : SapphireModule
        {
            detector_t detector;
            EnvPortMode envPortMode = EnvPortMode::Linear;

            EnvModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configControlGroup("Threshold", THRESHOLD_PARAM, THRESHOLD_ATTEN, THRESHOLD_CV_INPUT, MinThreshold, MaxThreshold, DefaultThreshold, " dB");
                configControlGroup("Speed", SPEED_PARAM, SPEED_ATTEN, SPEED_CV_INPUT, 0, 1, 0.5);
                configControlGroup("Center frequency", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT, -Gravy::OctaveRange, +Gravy::OctaveRange, Gravy::DefaultFrequencyKnob);
                configControlGroup("Resonance", RES_PARAM, RES_ATTEN, RES_CV_INPUT, 0, 1, 0.25);
                configInput(AUDIO_INPUT, "Audio");
                configOutput(ENVELOPE_OUTPUT, "Envelope");
                configOutput(PITCH_OUTPUT, "Pitch V/OCT");
                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void initialize()
            {
                detector.initialize();
                envPortMode = EnvPortMode::Linear;
            }

            json_t* dataToJson() override
            {
                json_t *root = SapphireModule::dataToJson();
                json_object_set_new(root, "envPortMode", json_integer(static_cast<int>(envPortMode)));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                json_t* epm = json_object_get(root, "envPortMode");
                if (json_is_integer(epm))
                    envPortMode = static_cast<EnvPortMode>(json_integer_value(epm));
            }

            void process(const ProcessArgs& args) override
            {
                int nc = inputs[AUDIO_INPUT].getChannels();
                nc = std::max(nc, inputs[FREQ_CV_INPUT].getChannels());
                nc = std::max(nc, inputs[RES_CV_INPUT].getChannels());
                nc = std::max(nc, inputs[SPEED_CV_INPUT].getChannels());
                nc = std::max(nc, inputs[THRESHOLD_CV_INPUT].getChannels());
                if (nc <= 0)
                {
                    const float zero = 0;
                    setPolyOutput(ENVELOPE_OUTPUT, 1, &zero);
                    setPolyOutput(PITCH_OUTPUT, 1, &zero);
                }
                else
                {
                    float inFrame[PORT_MAX_CHANNELS];
                    float outEnvelope[PORT_MAX_CHANNELS];
                    float outPitchVoct[PORT_MAX_CHANNELS];

                    float audio = 0;
                    float cvFreq = 0;
                    float cvRes = 0;
                    float cvSpeed = 0;
                    float cvThresh = 0;
                    float thresh[PORT_MAX_CHANNELS]{};

                    for (int c = 0; c < nc; ++c)
                    {
                        inFrame[c] = nextChannelInputVoltage(audio, AUDIO_INPUT, c);

                        nextChannelInputVoltage(cvFreq, FREQ_CV_INPUT, c);
                        float freq = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, cvFreq, -Gravy::OctaveRange, +Gravy::OctaveRange);
                        detector.setFrequency(freq, c);

                        nextChannelInputVoltage(cvRes, RES_CV_INPUT, c);
                        float res = cvGetControlValue(RES_PARAM, RES_ATTEN, cvRes, 0, 1);
                        detector.setResonance(res, c);

                        nextChannelInputVoltage(cvSpeed, SPEED_CV_INPUT, c);
                        float speed = cvGetControlValue(SPEED_PARAM, SPEED_ATTEN, cvSpeed, 0, 1);
                        detector.setSpeed(speed, c);

                        nextChannelInputVoltage(cvThresh, THRESHOLD_CV_INPUT, c);
                        float threshKnob = cvGetControlValue(THRESHOLD_PARAM, THRESHOLD_ATTEN, cvThresh, MinThreshold, MaxThreshold);
                        thresh[c] = detector.setThreshold(threshKnob, c);
                    }

                    int np = detector.process(nc, args.sampleRate, inFrame, outEnvelope, outPitchVoct);
                    assert(np == nc);

                    for (int c = 0; c < nc; ++c)
                        outEnvelope[c] = outputVoltage(outEnvelope[c], thresh[c]);

                    setPolyOutput(ENVELOPE_OUTPUT, nc, outEnvelope);
                    setPolyOutput(PITCH_OUTPUT, nc, outPitchVoct);
                }
            }

            void setPolyOutput(OutputId id, int nc, const float* volts)
            {
                outputs[id].setChannels(nc);
                for (int c = 0; c < nc; ++c)
                    outputs[id].setVoltage(volts[c], c);
            }

            float outputVoltage(float envelope, float threshold) const
            {
                switch (envPortMode)
                {
                case EnvPortMode::Linear:
                default:
                    return envelope;

                case EnvPortMode::GateActiveHi:
                    return (envelope < threshold) ? 0 : 10;

                case EnvPortMode::GateActiveLo:
                    return (envelope < threshold) ? 10 : 0;
                }
            }
        };

        void AddPortModesToMenu(Menu* menu, EnvModule* envModule)
        {
            if (menu == nullptr)
                return;

            if (envModule == nullptr)
                return;

            menu->addChild(createIndexSubmenuItem(
                "ENV port output mode",
                {
                    "Linear envelope",
                    "Gate when pitch detected",
                    "Gate when quiet"
                },
                [=]() { return static_cast<std::size_t>(envModule->envPortMode); },
                [=](size_t mode) { envModule->envPortMode = static_cast<EnvPortMode>(mode); }
            ));
        }

        struct EnvOutputPort : SapphirePort
        {
            EnvModule* envModule = nullptr;

            void appendContextMenu(ui::Menu* menu) override
            {
                SapphirePort::appendContextMenu(menu);
                menu->addChild(new MenuSeparator);
                AddPortModesToMenu(menu, envModule);
            }
        };

        struct EnvWidget : SapphireWidget
        {
            EnvModule* envModule{};

            explicit EnvWidget(EnvModule* module)
                : SapphireWidget("env", asset::plugin(pluginInstance, "res/env.svg"))
                , envModule(module)
            {
                setModule(module);
                addSapphireInput(AUDIO_INPUT, "audio_input");
                EnvOutputPort *ep = addSapphireOutput<EnvOutputPort>(ENVELOPE_OUTPUT, "envelope_output");
                ep->envModule = module;
                addSapphireOutput(PITCH_OUTPUT, "pitch_output");
                addSapphireFlatControlGroup("thresh", THRESHOLD_PARAM, THRESHOLD_ATTEN, THRESHOLD_CV_INPUT);
                addSapphireFlatControlGroup("speed", SPEED_PARAM, SPEED_ATTEN, SPEED_CV_INPUT);
                addSapphireFlatControlGroup("frequency", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
                addSapphireFlatControlGroup("resonance", RES_PARAM, RES_ATTEN, RES_CV_INPUT);
            }

            void appendContextMenu(Menu* menu) override
            {
                if (envModule == nullptr)
                    return;

                menu->addChild(new MenuSeparator);
                menu->addChild(envModule->createToggleAllSensitivityMenuItem());
                AddPortModesToMenu(menu, envModule);
            }
        };
    }
}


Model *modelSapphireEnv = createSapphireModel<Sapphire::Env::EnvModule, Sapphire::Env::EnvWidget>(
    "Env",
    Sapphire::ExpanderRole::None
);
