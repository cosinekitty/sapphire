#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Pop
    {
        enum ParamId
        {
            SPEED_PARAM,
            SPEED_ATTEN,
            CHAOS_PARAM,
            CHAOS_ATTEN,

            PARAMS_LEN
        };

        enum InputId
        {
            SPEED_CV_INPUT,
            CHAOS_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            TRIGGER_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct PopModule : SapphireModule
        {
            PopModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configOutput(TRIGGER_OUTPUT, "Trigger");

                configParam(SPEED_PARAM, -7, +7, 0, "Speed");
                configParam(CHAOS_PARAM, 0, 1, 1, "Chaos");

                configParam(SPEED_ATTEN, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(CHAOS_ATTEN, -1, 1, 0, "Chaos attenuverter", "%", 0, 100);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(CHAOS_CV_INPUT, "Chaos CV");

                initialize();
            }

            void initialize()
            {
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
            }
        };


        struct PopWidget : SapphireReloadableModuleWidget
        {
            explicit PopWidget(PopModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/pop.svg"))
            {
                setModule(module);

                addSapphireOutput(TRIGGER_OUTPUT, "trigger_output");

                addKnob(SPEED_PARAM, "speed_knob");
                addKnob(CHAOS_PARAM, "chaos_knob");

                addSapphireAttenuverter(SPEED_ATTEN, "speed_atten");
                addSapphireAttenuverter(CHAOS_ATTEN, "chaos_atten");

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(CHAOS_CV_INPUT, "chaos_cv");

                reloadPanel();
            }
        };
    }
}


Model* modelSapphirePop = createSapphireModel<Sapphire::Pop::PopModule, Sapphire::Pop::PopWidget>(
    "Pop",
    Sapphire::VectorRole::None
);
