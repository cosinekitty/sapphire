#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sauce_engine.hpp"

// Sapphire Sauce for VCV Rack by Don Cross <cosinekitty@gmail.com>.
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Sauce
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
            CASCADE_PARAM,
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
            AUDIO_NOTCH_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        constexpr unsigned SauceFilterStages = 3;
        using cascade_filter_t = CascadeStateVariableFilter<float, SauceFilterStages>;
        using sauce_engine_t = Gravy::SingleChannelGravyEngine<float, cascade_filter_t>;

        struct SauceModule : SapphireModule
        {
            sauce_engine_t engine[PORT_MAX_CHANNELS];
            AutomaticGainLimiter agcLow;
            AutomaticGainLimiter agcBand;
            AutomaticGainLimiter agcHigh;
            AutomaticGainLimiter agcNotch;
            bool enableAgc = false;

            SauceModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                using namespace Gravy;

                provideStereoSplitter = true;
                provideStereoMerge = true;
                shouldOfferFireDrill = true;

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                addAgcLevelQuantity(AGC_LEVEL_PARAM, 5, 50.5, 50, 50.5, 51.0);

                configInput(AUDIO_INPUT, "Audio");
                configOutput(AUDIO_LOWPASS_OUTPUT,  "Lowpass");
                configOutput(AUDIO_BANDPASS_OUTPUT, "Bandpass");
                configOutput(AUDIO_HIGHPASS_OUTPUT, "Highpass");
                configOutput(AUDIO_NOTCH_OUTPUT,    "Notch");

                configControlGroup("Frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT,  -OctaveRange, +OctaveRange, DefaultFrequencyKnob);
                configControlGroup("Resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT,   0, 1, DefaultResonanceKnob);
                configControlGroup("Mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT,   0, 1, DefaultMixKnob);
                configControlGroup("Gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT,  0, 1, DefaultGainKnob);

                configBypass(AUDIO_INPUT, AUDIO_LOWPASS_OUTPUT);
                configBypass(AUDIO_INPUT, AUDIO_BANDPASS_OUTPUT);
                configBypass(AUDIO_INPUT, AUDIO_HIGHPASS_OUTPUT);
                configBypass(AUDIO_INPUT, AUDIO_NOTCH_OUTPUT);

                configParam(CASCADE_PARAM, 1, 3, 1, "Cascade");

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
                    engine[c].initialize();
                reflectAgcSlider();
            }

            bool getAgcEnabled() const { return enableAgc; }

            void setAgcEnabled(bool enable)
            {
                if (enable && !enableAgc)
                {
                    // If the AGC isn't enabled, and caller wants to enable it,
                    // re-initialize the AGC so it forgets any previous level it had settled on.
                    agcLow.initialize();
                    agcBand.initialize();
                    agcHigh.initialize();
                    agcNotch.initialize();
                }
                enableAgc = enable;
            }

            double getAgcDistortion() override     // returns 0 when no distortion, or a positive value correlated with AGC distortion
            {
                if (!enableAgc)
                    return 0.0;

                // Only report distortion for connected output ports.
                // Otherwise, we could confuse people when the knob starts glowing for an unknown reason!

                double follower = 1.0;

                if (outputs.at(AUDIO_LOWPASS_OUTPUT).isConnected())
                    follower = std::max(follower, agcLow.getFollower());

                if (outputs.at(AUDIO_BANDPASS_OUTPUT).isConnected())
                    follower = std::max(follower, agcBand.getFollower());

                if (outputs.at(AUDIO_HIGHPASS_OUTPUT).isConnected())
                    follower = std::max(follower, agcHigh.getFollower());

                if (outputs.at(AUDIO_NOTCH_OUTPUT).isConnected())
                    follower = std::max(follower, agcNotch.getFollower());

                return follower - 1.0;
            }

            void reflectAgcSlider()
            {
                // Check for changes to the automatic gain control: its level, and whether enabled/disabled.
                if (agcLevelQuantity && agcLevelQuantity->changed)
                {
                    bool enabled = agcLevelQuantity->isAgcEnabled();
                    if (enabled)
                    {
                        float ceiling = agcLevelQuantity->clampedAgc();
                        agcLow.setCeiling(ceiling);
                        agcBand.setCeiling(ceiling);
                        agcHigh.setCeiling(ceiling);
                        agcNotch.setCeiling(ceiling);
                    }
                    setAgcEnabled(enabled);
                    agcLevelQuantity->changed = false;
                }
            }

            void process(const ProcessArgs& args) override
            {
                using namespace Gravy;

                float lpOutput[PORT_MAX_CHANNELS];
                float bpOutput[PORT_MAX_CHANNELS];
                float hpOutput[PORT_MAX_CHANNELS];
                float nxOutput[PORT_MAX_CHANNELS];

                const int nc = numOutputChannels(INPUTS_LEN, 0);

                if (limiterRecoveryCountdown > 0)
                {
                    // Continue to silence the output for the remainder of the reset period.
                    --limiterRecoveryCountdown;

                    for (int c = 0; c < nc; ++c)
                        lpOutput[c] = bpOutput[c] = hpOutput[c] = nxOutput[c] = 0;
                }
                else
                {
                    reflectAgcSlider();

                    float input = 0;
                    float cvFreq = 0;
                    float cvRes = 0;
                    float cvMix = 0;
                    float cvGain = 0;

                    // Reduce calculations for output ports that are not connected.
                    unsigned mask = 0;
                    if (outputs.at(AUDIO_LOWPASS_OUTPUT).isConnected())   mask |= NEED_LP;
                    if (outputs.at(AUDIO_BANDPASS_OUTPUT).isConnected())  mask |= NEED_BP;
                    if (outputs.at(AUDIO_HIGHPASS_OUTPUT).isConnected())  mask |= NEED_HP;
                    if (outputs.at(AUDIO_NOTCH_OUTPUT).isConnected())     mask |= NEED_NX;

                    if (mask == 0)
                        return;     // no need to do anything when no cables are connected to any output port.

                    // CASCADE is a manual knob only, not a control group. There is no CV input.
                    const float cascade = params.at(CASCADE_PARAM).getValue();

                    for (int c = 0; c < nc; ++c)
                    {
                        engine[c].filter.mask = mask;
                        engine[c].filter.setCascade(cascade);

                        nextChannelInputVoltage(input,  AUDIO_INPUT,   c);
                        nextChannelInputVoltage(cvFreq, FREQ_CV_INPUT, c);
                        nextChannelInputVoltage(cvRes,  RES_CV_INPUT,  c);
                        nextChannelInputVoltage(cvMix,  MIX_CV_INPUT,  c);
                        nextChannelInputVoltage(cvGain, GAIN_CV_INPUT, c);

                        float freqKnob  = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN,  cvFreq, -OctaveRange, +OctaveRange);
                        float resKnob   = cvGetControlValue(RES_PARAM,   RES_ATTEN,   cvRes);
                        float mixKnob   = cvGetControlValue(MIX_PARAM,   MIX_ATTEN,   cvMix);
                        float gainKnob  = cvGetControlValue(GAIN_PARAM,  GAIN_ATTEN,  cvGain);

                        engine[c].setFrequency(freqKnob);
                        engine[c].setResonance(resKnob);
                        engine[c].setMix(mixKnob);
                        engine[c].setGain(gainKnob);

                        FilterResult<float> result = engine[c].process(args.sampleRate, input);
                        lpOutput[c] = result.lowpass;
                        bpOutput[c] = result.bandpass;
                        hpOutput[c] = result.highpass;
                        nxOutput[c] = result.notch;
                    }

                    if (isFireDrillOneShot())
                    {
                        for (int k = 0; k < nc; ++k)
                            lpOutput[k] = bpOutput[k] = hpOutput[k] = nxOutput[k] = NAN;
                    }

                    if (enableAgc)
                    {
                        agcLow .process(args.sampleRate,  nc, lpOutput);
                        agcBand.process(args.sampleRate,  nc, bpOutput);
                        agcHigh.process(args.sampleRate,  nc, hpOutput);
                        agcNotch.process(args.sampleRate, nc, nxOutput);
                    }

                    if (isBadOutput(lpOutput, nc) || isBadOutput(bpOutput, nc) || isBadOutput(hpOutput, nc))
                    {
                        clearOutput(lpOutput, PORT_MAX_CHANNELS);
                        clearOutput(bpOutput, PORT_MAX_CHANNELS);
                        clearOutput(hpOutput, PORT_MAX_CHANNELS);
                        clearOutput(nxOutput, PORT_MAX_CHANNELS);

                        for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                            engine[c].initialize();

                        beginRecovery(args.sampleRate);
                    }
                }

                outputs.at(AUDIO_LOWPASS_OUTPUT ).setChannels(nc);
                outputs.at(AUDIO_BANDPASS_OUTPUT).setChannels(nc);
                outputs.at(AUDIO_HIGHPASS_OUTPUT).setChannels(nc);
                outputs.at(AUDIO_NOTCH_OUTPUT   ).setChannels(nc);

                for (int c = 0; c < nc; ++c)
                {
                    outputs.at(AUDIO_LOWPASS_OUTPUT ).setVoltage(lpOutput[c], c);
                    outputs.at(AUDIO_BANDPASS_OUTPUT).setVoltage(bpOutput[c], c);
                    outputs.at(AUDIO_HIGHPASS_OUTPUT).setVoltage(hpOutput[c], c);
                    outputs.at(AUDIO_NOTCH_OUTPUT   ).setVoltage(nxOutput[c], c);
                }
            }
        };


        struct SauceWidget : SapphireWidget
        {
            SauceModule* sauceModule{};

            explicit SauceWidget(SauceModule* module)
                : SapphireWidget("sauce", asset::plugin(pluginInstance, "res/sauce.svg"))
                , sauceModule(module)
            {
                setModule(module);

                addSapphireInput(AUDIO_INPUT, "audio_input");
                addSapphireOutput(AUDIO_LOWPASS_OUTPUT,  "audio_lp_output");
                addSapphireOutput(AUDIO_BANDPASS_OUTPUT, "audio_bp_output");
                addSapphireOutput(AUDIO_HIGHPASS_OUTPUT, "audio_hp_output");
                addSapphireOutput(AUDIO_NOTCH_OUTPUT,    "audio_notch_output");
                addCascadeKnob();
                addSnapVoctFlatControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT );
                addSapphireFlatControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                addSapphireFlatControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                addSapphireFlatControlGroupWithWarningLight("gain", GAIN_PARAM, GAIN_ATTEN, GAIN_CV_INPUT);
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
                if (sauceModule)
                {
                    menu->addChild(sauceModule->createToggleAllSensitivityMenuItem());
                }
            }

            void addCascadeKnob()
            {
                addSmallKnob<Trimpot>(CASCADE_PARAM, "cascade_knob");
            }
        };
    }
}


Model *modelSapphireSauce = createSapphireModel<Sapphire::Sauce::SauceModule, Sapphire::Sauce::SauceWidget>(
    "Sauce",
    Sapphire::ExpanderRole::None
);
