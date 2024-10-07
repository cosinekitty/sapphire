#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
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

            GravyModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                provideStereoSplitter = true;
                provideStereoMerge = true;

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(AUDIO_LEFT_INPUT,  "Audio left");
                configInput(AUDIO_RIGHT_INPUT, "Audio right");

                configOutput(AUDIO_LEFT_OUTPUT,  "Audio left");
                configOutput(AUDIO_RIGHT_OUTPUT, "Audio right");

                configControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT,  -OctaveRange, +OctaveRange, DefaultFrequencyKnob);
                configControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT,   0, 1, DefaultResonanceKnob);
                configControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT,   0, 1, DefaultMixKnob);
                configControlGroup("gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT,  0, 1, DefaultGainKnob);

                configSwitch(FILTER_MODE_PARAM, 0.0, 2.0, 1.0, "Mode", {"Lowpass", "Bandpass", "Highpass"});

                configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
                configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);

                initialize();
            }

            void initialize()
            {
                engine.initialize();
            }

            FilterMode getFilterMode()
            {
                return static_cast<FilterMode>(params[FILTER_MODE_PARAM].getValue());
            }

            void process(const ProcessArgs& args) override
            {
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

                float inFrame[2];
                loadStereoInputs(inFrame[0], inFrame[1], AUDIO_LEFT_INPUT, AUDIO_RIGHT_INPUT);

                float outFrame[2];
                engine.process(args.sampleRate, inFrame, outFrame);

                writeStereoOutputs(outFrame[0], outFrame[1], AUDIO_LEFT_OUTPUT, AUDIO_RIGHT_OUTPUT);
            }
        };


        struct GravyWidget : SapphireWidget
        {
            GravyModule *gravyModule{};

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
                addSapphireFlatControlGroup("gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT );

                CKSSThreeHorizontal* modeSwitch = createParamCentered<CKSSThreeHorizontal>(Vec{}, module, FILTER_MODE_PARAM);
                addReloadableParam(modeSwitch, "mode_switch");

                loadStereoMergeLabels();
            }

            void appendContextMenu(Menu* menu) override
            {
                if (gravyModule == nullptr)
                    return;

                menu->addChild(new MenuSeparator);
                menu->addChild(gravyModule->createToggleAllSensitivityMenuItem());
                menu->addChild(gravyModule->createStereoSplitterMenuItem());
                menu->addChild(gravyModule->createStereoMergeMenuItem());
            }
        };
    }
}


Model *modelSapphireGravy = createSapphireModel<Sapphire::Gravy::GravyModule, Sapphire::Gravy::GravyWidget>(
    "Gravy",
    Sapphire::VectorRole::None
);
