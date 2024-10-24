#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "gravy_engine.hpp"

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
            FILTER_MODE_PARAM,
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
            AUDIO_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        using poly_gravy_engine_t = Gravy::GravyEngine<PORT_MAX_CHANNELS>;

        struct PolyGravyModule : SapphireModule
        {
            poly_gravy_engine_t engine;
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
                configOutput(AUDIO_OUTPUT, "Audio");

                configControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT,  -OctaveRange, +OctaveRange, DefaultFrequencyKnob);
                configControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT,   0, 1, DefaultResonanceKnob);
                configControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT,   0, 1, DefaultMixKnob);
                configControlGroup("gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT,  0, 1, DefaultGainKnob);

                configSwitch(FILTER_MODE_PARAM, 0.0, 2.0, 1.0, "Mode", {"Lowpass", "Bandpass", "Highpass"});

                configBypass(AUDIO_INPUT, AUDIO_OUTPUT);

                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void initialize()
            {
                engine.initialize();
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

            FilterMode getFilterMode()
            {
                return static_cast<FilterMode>(params[FILTER_MODE_PARAM].getValue());
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
                float output[PORT_MAX_CHANNELS]{};

                if (limiterRecoveryCountdown > 0)
                {
                    // Continue to silence the output for the remainder of the reset period.
                    --limiterRecoveryCountdown;
                }
                else
                {
                    reflectAgcSlider();

                    //!!! add polyphonic logic here

                    float freqKnob  = getControlValue(FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT, -OctaveRange, +OctaveRange);
                    float resKnob   = getControlValue(RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                    float mixKnob   = getControlValue(MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                    float gainKnob  = getControlValue(GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT );
                    FilterMode mode = getFilterMode();

                    engine.setFilterMode(mode);
                    engine.setFrequency(freqKnob);
                    engine.setResonance(resKnob);
                    engine.setMix(mixKnob);
                    engine.setGain(gainKnob);

                    float input[PORT_MAX_CHANNELS]{};

                    engine.process(args.sampleRate, nc, input, output);

                    if (enableAgc)
                        agc.process(args.sampleRate, nc, output);

                    if (checkOutputs(args.sampleRate, output, nc))
                        engine.initialize();
                }

                outputs[AUDIO_OUTPUT].setChannels(nc);
                for (int c = 0; c < nc; ++c)
                    outputs[AUDIO_OUTPUT].setVoltage(output[c], c);
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
                addSapphireOutput(AUDIO_OUTPUT, "audio_output");

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
