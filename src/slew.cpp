#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_slew_engine.hpp"

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
            VELOCITY_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct SlewModule : SapphireModule
        {
            Slew::Engine engine[PORT_MAX_CHANNELS];

            SlewModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(TARGET_INPUT, "Target");
                configOutput(SLEW_OUTPUT, "Slew");
                configOutput(VELOCITY_OUTPUT, "Velocity");

                configParam(SPEED_KNOB_PARAM, MinSpeed, MaxSpeed, DefSpeed, "Speed");
                configParam(SPEED_ATTEN_PARAM, -1, +1, 0, "Speed attenuverter", "%", 0, 100);
                configInput(SPEED_CV_INPUT, "Speed CV");

                configParam(VISCOSITY_KNOB_PARAM, MinViscosity, MaxViscosity, DefViscosity, "Viscosity");
                configParam(VISCOSITY_ATTEN_PARAM, -1, +1, 0, "Viscosity attenuverter", "%", 0, 100);
                configInput(VISCOSITY_CV_INPUT, "Viscosity CV");

                initialize();
            }

            void initialize()
            {
                for (int i = 0; i < PORT_MAX_CHANNELS; ++i)
                    engine[i].initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                const float VELOCITY_DENOM = 1000;      // velocity signals get very hot because they are [m/s]
                const int nc = numOutputChannels(INPUTS_LEN);
                outputs[SLEW_OUTPUT].setChannels(nc);
                outputs[VELOCITY_OUTPUT].setChannels(nc);

                float vTarget = 0;
                float cvSpeed = 0;
                float cvViscosity = 0;
                for (int c = 0; c < nc; ++c)
                {
                    nextChannelInputVoltage(vTarget, TARGET_INPUT, c);
                    nextChannelInputVoltage(cvSpeed, SPEED_CV_INPUT, c);
                    nextChannelInputVoltage(cvViscosity, VISCOSITY_CV_INPUT, c);
                    float speed = cvGetControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN_PARAM, cvSpeed, MinSpeed, MaxSpeed);
                    float viscosity = cvGetControlValue(VISCOSITY_KNOB_PARAM, VISCOSITY_ATTEN_PARAM, cvViscosity, MinViscosity, MaxViscosity);
                    float dt = args.sampleTime * std::pow(2.0f, speed);
                    auto particle = engine[c].process(dt, vTarget, viscosity);
                    outputs[SLEW_OUTPUT].setVoltage(particle.r, c);
                    outputs[VELOCITY_OUTPUT].setVoltage(particle.v / VELOCITY_DENOM, c);
                }
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
                addSapphireOutput(VELOCITY_OUTPUT, "velocity_output");
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
