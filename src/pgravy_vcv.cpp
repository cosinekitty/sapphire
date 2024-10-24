#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "pgravy_engine.hpp"

// Sapphire Gravy for VCV Rack by Don Cross <cosinekitty@gmail.com>.
// A biquad filter implementation that supports tunable frequency and resonance.
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace PolyGravy
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
            AGC_LEVEL_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            FREQ_CV_INPUT,
            RES_CV_INPUT,
            MIX_CV_INPUT,
            GAIN_CV_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_LOWPASS_OUTPUT,
            AUDIO_BANDPASS_OUTPUT,
            AUDIO_HIGHPASS_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct PolyGravyModule : SapphireModule
        {
            Gravy::SingleChannelGravyEngine engine[PORT_MAX_CHANNELS];
            AgcLevelQuantity *agcLevelQuantity{};
            AutomaticGainLimiter agc;
            bool enableAgc = false;

            PolyGravyModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                using namespace Gravy;

                provideStereoSplitter = true;
                provideStereoMerge = true;

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                agcLevelQuantity = makeAgcLevelQuantity(AGC_LEVEL_PARAM, 5, 50.5, 50, 50.5, 51.0);

                configInput(AUDIO_INPUT, "Audio");
                configOutput(AUDIO_LOWPASS_OUTPUT,  "Lowpass");
                configOutput(AUDIO_BANDPASS_OUTPUT, "Bandpass");
                configOutput(AUDIO_HIGHPASS_OUTPUT, "Highpass");

                configControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT,  -OctaveRange, +OctaveRange, DefaultFrequencyKnob);
                configControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT,   0, 1, DefaultResonanceKnob);
                configControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT,   0, 1, DefaultMixKnob);
                configControlGroup("gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT,  0, 1, DefaultGainKnob);

                configBypass(AUDIO_INPUT, AUDIO_LOWPASS_OUTPUT);
                configBypass(AUDIO_INPUT, AUDIO_BANDPASS_OUTPUT);
                configBypass(AUDIO_INPUT, AUDIO_HIGHPASS_OUTPUT);

                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void initialize()
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    engine[c].initialize();
                agcLevelQuantity->initialize();
                reflectAgcSlider();
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

            double getAgcDistortion() const override     // returns 0 when no distortion, or a positive value correlated with AGC distortion
            {
                return enableAgc ? (agc.getFollower() - 1.0) : 0.0;
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                json_object_set_new(root, "limiterWarningLight", json_boolean(enableLimiterWarning));
                agcLevelQuantity->save(root, "agcLevel");
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);

                json_t *warningFlag = json_object_get(root, "limiterWarningLight");
                enableLimiterWarning = !json_is_false(warningFlag);

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
                using namespace Gravy;

                const int nc = numOutputChannels(INPUTS_LEN);
                float output[3*PORT_MAX_CHANNELS];    // lowpass, bandpass, highpass.

                if (limiterRecoveryCountdown > 0)
                {
                    // Continue to silence the output for the remainder of the reset period.
                    --limiterRecoveryCountdown;

                    for (int c = 0; c < 3*PORT_MAX_CHANNELS; ++c)
                        output[c] = 0;
                }
                else
                {
                    reflectAgcSlider();

                    float input = 0;
                    float cvFreq = 0;
                    float cvRes = 0;
                    float cvMix = 0;
                    float cvGain = 0;

                    for (int c = 0; c < nc; ++c)
                    {
                        nextChannelInputVoltage(input,  AUDIO_INPUT,   c);
                        nextChannelInputVoltage(cvFreq, FREQ_CV_INPUT, c);
                        nextChannelInputVoltage(cvRes,  RES_CV_INPUT,  c);
                        nextChannelInputVoltage(cvMix,  MIX_CV_INPUT,  c);
                        nextChannelInputVoltage(cvGain, GAIN_CV_INPUT, c);

                        float freqKnob  = cvGetControlValue(FREQ_PARAM,  FREQ_ATTEN,  cvFreq, -OctaveRange, +OctaveRange);
                        float resKnob   = cvGetControlValue(RES_PARAM,   RES_ATTEN,   cvRes);
                        float mixKnob   = cvGetControlValue(MIX_PARAM,   MIX_ATTEN,   cvMix);
                        float gainKnob  = cvGetControlValue(GAIN_PARAM,  GAIN_ATTEN,  cvGain);

                        engine[c].setFrequency(freqKnob);
                        engine[c].setResonance(resKnob);
                        engine[c].setMix(mixKnob);
                        engine[c].setGain(gainKnob);

                        FilterResult<float> result = engine[c].process(args.sampleRate, input);
                        output[c + 0*PORT_MAX_CHANNELS] = result.lowpass;
                        output[c + 1*PORT_MAX_CHANNELS] = result.bandpass;
                        output[c + 2*PORT_MAX_CHANNELS] = result.highpass;
                    }

                    for (int c = nc; c < PORT_MAX_CHANNELS; ++c)
                    {
                        output[c + 0*PORT_MAX_CHANNELS] = 0;
                        output[c + 1*PORT_MAX_CHANNELS] = 0;
                        output[c + 2*PORT_MAX_CHANNELS] = 0;
                    }

                    if (enableAgc)
                    {
                        agc.process(args.sampleRate, 3*PORT_MAX_CHANNELS, output);
                    }

                    if (checkOutputs(args.sampleRate, output, 3*PORT_MAX_CHANNELS))
                    {
                        for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                            engine[c].initialize();
                    }
                }

                outputs[AUDIO_LOWPASS_OUTPUT ].setChannels(nc);
                outputs[AUDIO_BANDPASS_OUTPUT].setChannels(nc);
                outputs[AUDIO_HIGHPASS_OUTPUT].setChannels(nc);
                for (int c = 0; c < nc; ++c)
                {
                    outputs[AUDIO_LOWPASS_OUTPUT ].setVoltage(output[c + 0*PORT_MAX_CHANNELS], c);
                    outputs[AUDIO_BANDPASS_OUTPUT].setVoltage(output[c + 1*PORT_MAX_CHANNELS], c);
                    outputs[AUDIO_HIGHPASS_OUTPUT].setVoltage(output[c + 2*PORT_MAX_CHANNELS], c);
                }
            }
        };


        struct PolyGravyWidget : SapphireWidget
        {
            PolyGravyModule* gravyModule{};
            WarningLightWidget* warningLight{};

            explicit PolyGravyWidget(PolyGravyModule* module)
                : SapphireWidget("pgravy", asset::plugin(pluginInstance, "res/pgravy.svg"))
                , gravyModule(module)
            {
                setModule(module);

                addSapphireInput(AUDIO_INPUT,  "audio_input");
                addSapphireOutput(AUDIO_LOWPASS_OUTPUT,  "audio_lp_output");
                addSapphireOutput(AUDIO_BANDPASS_OUTPUT, "audio_bp_output");
                addSapphireOutput(AUDIO_HIGHPASS_OUTPUT, "audio_hp_output");

                addSapphireFlatControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT );
                addSapphireFlatControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                addSapphireFlatControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                auto gainKnob = addSapphireFlatControlGroup("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);

                warningLight = new WarningLightWidget(module);
                warningLight->box.pos  = Vec(0.0f, 0.0f);
                warningLight->box.size = gainKnob->box.size;
                gainKnob->addChild(warningLight);
            }

            void appendContextMenu(Menu* menu) override
            {
                if (gravyModule == nullptr)
                    return;

                menu->addChild(new MenuSeparator);
                menu->addChild(gravyModule->createToggleAllSensitivityMenuItem());
                menu->addChild(new AgcLevelSlider(gravyModule->agcLevelQuantity));
                menu->addChild(createBoolPtrMenuItem<bool>("Limiter warning light", "", &gravyModule->enableLimiterWarning));
            }
        };
    }
}


Model *modelSapphirePGravy = createSapphireModel<Sapphire::PolyGravy::PolyGravyModule, Sapphire::PolyGravy::PolyGravyWidget>(
    "PGravy",
    Sapphire::VectorRole::None
);
