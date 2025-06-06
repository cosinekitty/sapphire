#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_smoother.hpp"
#include "gravy_engine.hpp"

// Sapphire Gravy for VCV Rack by Don Cross <cosinekitty@gmail.com>.
// A biquad filter implementation that supports tunable frequency and resonance.
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Gravy
    {
        enum ParamId
        {
            FREQ_PARAM,
            FREQ_ATTEN,
            RES_PARAM,
            RES_ATTEN,
            MIX_PARAM,
            MIX_ATTEN,
            GAIN_PARAM,
            GAIN_ATTEN,
            FILTER_MODE_PARAM,
            AGC_LEVEL_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_LEFT_INPUT,
            AUDIO_RIGHT_INPUT,
            FREQ_CV_INPUT,
            RES_CV_INPUT,
            MIX_CV_INPUT,
            GAIN_CV_INPUT,
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

        using gravy_engine_t = GravyEngine<2>;

        struct GravyModule : SapphireModule
        {
            gravy_engine_t engine;
            AgcLevelQuantity *agcLevelQuantity{};
            AutomaticGainLimiter agc;
            bool enableAgc = false;
            EnumSmoother<FilterMode> smoother{FilterMode::Default, "", 0.01};

            GravyModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                provideStereoSplitter = true;
                provideStereoMerge = true;

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                agcLevelQuantity = makeAgcLevelQuantity(AGC_LEVEL_PARAM, 5, 50.5, 50, 50.5, 51.0);

                configInput(AUDIO_LEFT_INPUT,  "Audio left");
                configInput(AUDIO_RIGHT_INPUT, "Audio right");

                configOutput(AUDIO_LEFT_OUTPUT,  "Audio left");
                configOutput(AUDIO_RIGHT_OUTPUT, "Audio right");

                configControlGroup("Frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT,  -OctaveRange, +OctaveRange, DefaultFrequencyKnob);
                configControlGroup("Resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT,   0, 1, DefaultResonanceKnob);
                configControlGroup("Mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT,   0, 1, DefaultMixKnob);
                configControlGroup("Gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT,  0, 1, DefaultGainKnob);

                configSwitch(FILTER_MODE_PARAM, 0.0, 2.0, 1.0, "Mode", {"Lowpass", "Bandpass", "Highpass"});

                configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
                configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);

                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void initialize()
            {
                engine.initialize();
                agcLevelQuantity->initialize();
                reflectAgcSlider();
                smoother.initialize();
            }

            bool getAgcEnabled() const { return enableAgc; }

            void setAgcEnabled(bool enable)
            {
                if (enable && !enableAgc)
                {
                    // If the AGC isn't enabled, and caller wants to enable it,
                    // re-initialize the AGC so it forgets any previous level it had settled on.
                    agc.initialize();
                }
                enableAgc = enable;
            }

            double getAgcDistortion() override     // returns 0 when no distortion, or a positive value correlated with AGC distortion
            {
                return enableAgc ? (agc.getFollower() - 1.0) : 0.0;
            }

            FilterMode getFilterMode()
            {
                return static_cast<FilterMode>(params.at(FILTER_MODE_PARAM).getValue());
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                agcLevelQuantity->save(root, "agcLevel");
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                agcLevelQuantity->load(root, "agcLevel");
            }

            void reflectAgcSlider()
            {
                // Check for changes to the automatic gain control: its level, and whether enabled/disabled.
                if (agcLevelQuantity && agcLevelQuantity->changed)
                {
                    bool enabled = agcLevelQuantity->isAgcEnabled();
                    if (enabled)
                        agc.setCeiling(agcLevelQuantity->clampedAgc());
                    setAgcEnabled(enabled);
                    agcLevelQuantity->changed = false;
                }
            }

            void process(const ProcessArgs& args) override
            {
                float output[2];

                if (limiterRecoveryCountdown > 0)
                {
                    // Continue to silence the output for the remainder of the reset period.
                    --limiterRecoveryCountdown;
                    output[0] = output[1] = 0;
                }
                else
                {
                    reflectAgcSlider();

                    float freqKnob  = getControlValueVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT, -OctaveRange, +OctaveRange);
                    float resKnob   = getControlValue(RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                    float mixKnob   = getControlValue(MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                    float gainKnob  = getControlValue(GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT );

                    smoother.targetValue = getFilterMode();
                    float smooth = smoother.process(args.sampleRate);

                    engine.setFilterMode(smoother.currentValue);
                    engine.setFrequency(freqKnob);
                    engine.setResonance(resKnob);
                    engine.setMix(mixKnob);
                    engine.setGain(gainKnob);

                    float input[2];
                    loadStereoInputs(input[0], input[1], AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);

                    engine.process(args.sampleRate, 2, input, output);
                    output[0] *= smooth;
                    output[1] *= smooth;

                    if (enableAgc)
                        agc.process(args.sampleRate, output[0], output[1]);

                    if (checkOutputs(args.sampleRate, output, 2))
                        engine.initialize();
                }

                writeStereoOutputs(output[0], output[1], AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT);
            }
        };


        struct GravyWidget : SapphireWidget
        {
            GravyModule* gravyModule{};
            WarningLightWidget* warningLight{};

            explicit GravyWidget(GravyModule* module)
                : SapphireWidget("gravy", asset::plugin(pluginInstance, "res/gravy.svg"))
                , gravyModule(module)
            {
                setModule(module);

                addSapphireInput(AUDIO_LEFT_INPUT,  "audio_left_input");
                addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");

                addSapphireOutput(AUDIO_LEFT_OUTPUT,  "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");

                addSapphireFlatControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT );
                addSapphireFlatControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                addSapphireFlatControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                auto gainKnob = addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);

                warningLight = new WarningLightWidget(module);
                warningLight->box.pos  = Vec(0.0f, 0.0f);
                warningLight->box.size = gainKnob->box.size;
                gainKnob->addChild(warningLight);

                CKSSThreeHorizontal* modeSwitch = createParamCentered<CKSSThreeHorizontal>(Vec{}, module, FILTER_MODE_PARAM);
                addSapphireParam(modeSwitch, "mode_switch");

                loadInputStereoLabels();
                loadOutputStereoLabels();
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
                if (gravyModule)
                {
                    menu->addChild(gravyModule->createToggleAllSensitivityMenuItem());
                    menu->addChild(gravyModule->createStereoSplitterMenuItem());
                    menu->addChild(gravyModule->createStereoMergeMenuItem());
                    menu->addChild(new MenuSeparator);
                    menu->addChild(new AgcLevelSlider(gravyModule->agcLevelQuantity));
                    gravyModule->addLimiterWarningLightOption(menu);
                }
            }
        };
    }
}


Model *modelSapphireGravy = createSapphireModel<Sapphire::Gravy::GravyModule, Sapphire::Gravy::GravyWidget>(
    "Gravy",
    Sapphire::ExpanderRole::None
);
