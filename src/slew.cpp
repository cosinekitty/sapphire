#include "plugin.hpp"
#include "sapphire_widget.hpp"

// Sapphire Slew for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>.
// Inertial slew limiter idea by Joe Sayer <joesayeruk@gmail.com>.
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Slew
    {
        enum ParamId
        {
            SPEED_KNOB_PARAM,
            SPEED_ATTEN_PARAM,

            VISCOSITY_KNOB_PARAM,
            VISCOSITY_ATTEN_PARAM,

            PARAMS_LEN
        };

        enum InputId
        {
            TARGET_INPUT,
            SPEED_CV_INPUT,
            VISCOSITY_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            SLEW_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct SlewModule : SapphireModule
        {
            SlewModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(TARGET_INPUT, "Target");
                configOutput(SLEW_OUTPUT, "Slew");

                configParam(SPEED_KNOB_PARAM, -7, +7, 0, "Speed");
                configParam(SPEED_ATTEN_PARAM, -1, +1, 0, "Speed attenuverter", "%", 0, 100);
                configInput(SPEED_CV_INPUT, "Speed CV");

                configParam(VISCOSITY_KNOB_PARAM, 0, 1, 0.5, "Viscosity");
                configParam(VISCOSITY_ATTEN_PARAM, -1, +1, 0, "Viscosity attenuverter", "%", 0, 100);
                configInput(VISCOSITY_CV_INPUT, "Viscosity CV");

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


        struct SlewWidget : SapphireReloadableModuleWidget
        {
            explicit SlewWidget(SlewModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/slew.svg"))
            {
                setModule(module);
                addSapphireInput(TARGET_INPUT, "target_input");
                addSapphireOutput(SLEW_OUTPUT, "slew_output");
                addSapphireControlGroup("speed", SPEED_KNOB_PARAM, SPEED_ATTEN_PARAM, SPEED_CV_INPUT);
                addSapphireControlGroup("viscosity", VISCOSITY_KNOB_PARAM, VISCOSITY_ATTEN_PARAM, VISCOSITY_CV_INPUT);
                reloadPanel();
            }
        };
    }
}

Model* modelSlew = createSapphireModel<Sapphire::Slew::SlewModule, Sapphire::Slew::SlewWidget>(
    "Slew",
    Sapphire::VectorRole::None
);
