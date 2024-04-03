#include "plugin.hpp"
#include "sapphire_spatula_engine.hpp"
#include "sapphire_widget.hpp"
#include "chaos.hpp"

namespace Sapphire
{
    namespace Bash
    {
        enum ParamId
        {
            SPEED_KNOB_PARAM,
            SPEED_ATTEN_PARAM,
            LEVEL_CHAOS_PARAM,
            LEVEL_KNOB_PARAM,
            CENTER_CHAOS_PARAM,
            CENTER_KNOB_PARAM,
            WIDTH_CHAOS_PARAM,
            WIDTH_KNOB_PARAM,
            DISPERSION_CHAOS_PARAM,
            DISPERSION_KNOB_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            SPEED_CV_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_OUTPUT,
            COPY_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct BashModule : SapphireModule
        {
            Aizawa chaos1 {+0.441, -0.782, -0.289};
            Aizawa chaos2 {-0.871, +0.248, +1.074};

            BashModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configParam(SPEED_KNOB_PARAM, -7, +7, 0, "Speed");
                configParam(SPEED_ATTEN_PARAM, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(AUDIO_INPUT, "Audio");
                configOutput(AUDIO_OUTPUT, "Audio");
                configOutput(COPY_OUTPUT, "Copy");
                initialize();
            }

            void initialize()
            {
                chaos1.initialize();
                chaos1.setKnob(0.5);
                chaos2.initialize();
                chaos2.setKnob(0.6);
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                float speed = getControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN_PARAM, SPEED_CV_INPUT, -7, +7);
                double dt = args.sampleTime * std::pow(2.0f, speed);
                chaos1.update(dt * 0.987);
                chaos2.update(dt);
            }
        };


        struct BashWidget : SapphireReloadableModuleWidget
        {
            explicit BashWidget(BashModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/bash.svg"))
            {
                setModule(module);
                addSapphireInput(AUDIO_INPUT, "audio_input");
                addSapphireControlGroup("speed", SPEED_KNOB_PARAM, SPEED_ATTEN_PARAM, SPEED_CV_INPUT);
                addSapphireChaosGroup("level", LEVEL_CHAOS_PARAM, LEVEL_KNOB_PARAM);
                addSapphireChaosGroup("center", CENTER_CHAOS_PARAM, CENTER_KNOB_PARAM);
                addSapphireChaosGroup("width", WIDTH_CHAOS_PARAM, WIDTH_KNOB_PARAM);
                addSapphireChaosGroup("dispersion", DISPERSION_CHAOS_PARAM, DISPERSION_KNOB_PARAM);
                addSapphireOutput(AUDIO_OUTPUT, "audio_output");
                addSapphireOutput(COPY_OUTPUT, "copy_output");
                reloadPanel();
            }
        };
    }
}

Model *modelBash = createSapphireModel<Sapphire::Bash::BashModule, Sapphire::Bash::BashWidget>(
    "Bash",
    Sapphire::VectorRole::None
);
