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
            DRIVE_PARAM,
            DRIVE_ATTEN,
            MIX_PARAM,
            MIX_ATTEN,
            GAIN_PARAM,
            GAIN_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_LEFT_INPUT,
            AUDIO_RIGHT_INPUT,
            FREQ_CV_INPUT,
            RES_CV_INPUT,
            DRIVE_CV_INPUT,
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

        using gravy_engine_t = PolyEngine<2>;

        struct GravyModule : SapphireModule
        {
            gravy_engine_t engine;

            GravyModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(AUDIO_LEFT_INPUT,  "Audio left");
                configInput(AUDIO_RIGHT_INPUT, "Audio right");

                configOutput(AUDIO_LEFT_OUTPUT,  "Audio left");
                configOutput(AUDIO_RIGHT_OUTPUT, "Audio right");

                configControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT,  -OctaveRange, +OctaveRange, DefaultFrequencyKnob);
                configControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT,   0, 1, DefaultResonanceKnob);
                configControlGroup("drive",     DRIVE_PARAM, DRIVE_ATTEN, DRIVE_CV_INPUT, 0, 1, DefaultDriveKnob);
                configControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT,   0, 1, DefaultMixKnob);
                configControlGroup("gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT,  0, 1, DefaultGainKnob);

                configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
                configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);

                initialize();
            }

            void initialize()
            {
                engine.initialize();
            }

            void process(const ProcessArgs& args) override
            {
                //float freqKnob  = getControlValue(FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT, -OctaveRange, +OctaveRange);
                //float resKnob   = getControlValue(RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                //float driveKnob = getControlValue(DRIVE_PARAM, DRIVE_ATTEN, DRIVE_CV_INPUT);
                //float mixKnob   = getControlValue(MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                //float gainKnob  = getControlValue(GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT );
            }
        };


        struct GravyWidget : SapphireReloadableModuleWidget
        {
            GravyModule *gravyModule{};

            explicit GravyWidget(GravyModule* module)
                : SapphireReloadableModuleWidget("gravy", asset::plugin(pluginInstance, "res/gravy.svg"))
                , gravyModule(module)
            {
                setModule(module);

                addSapphireInput(AUDIO_LEFT_INPUT,  "audio_left_input");
                addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");

                addSapphireOutput(AUDIO_LEFT_OUTPUT,  "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");

                addSapphireFlatControlGroup("frequency", FREQ_PARAM,  FREQ_ATTEN,  FREQ_CV_INPUT );
                addSapphireFlatControlGroup("resonance", RES_PARAM,   RES_ATTEN,   RES_CV_INPUT  );
                addSapphireFlatControlGroup("drive",     DRIVE_PARAM, DRIVE_ATTEN, DRIVE_CV_INPUT);
                addSapphireFlatControlGroup("mix",       MIX_PARAM,   MIX_ATTEN,   MIX_CV_INPUT  );
                addSapphireFlatControlGroup("gain",      GAIN_PARAM,  GAIN_ATTEN,  GAIN_CV_INPUT );
            }
        };
    }
}


Model *modelSapphireGravy = createSapphireModel<Sapphire::Gravy::GravyModule, Sapphire::Gravy::GravyWidget>(
    "Gravy",
    Sapphire::VectorRole::None
);
