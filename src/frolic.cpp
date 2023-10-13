#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "chaos.hpp"
#include "tricorder.hpp"

// Frolic for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Frolic
    {
        enum ParamId
        {
            SPEED_KNOB_PARAM,
            CHAOS_KNOB_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            INPUTS_LEN
        };

        enum OutputId
        {
            X_OUTPUT,
            Y_OUTPUT,
            Z_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct FrolicModule : Module
        {
            Rucklidge circuit;
            Tricorder::Message tricorderMessage[2];

            FrolicModule()
            {
                rightExpander.producerMessage = &tricorderMessage[0];
                rightExpander.consumerMessage = &tricorderMessage[1];

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");

                configParam(SPEED_KNOB_PARAM, -7, +7, 0, "Speed");
                configParam(CHAOS_KNOB_PARAM, -1, +1, 0, "Chaos adjust");

                initialize();
            }

            void initialize()
            {
                circuit.initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                circuit.setKnob(params[CHAOS_KNOB_PARAM].getValue());
                double dt = args.sampleTime * std::pow(2.0f, params[SPEED_KNOB_PARAM].getValue());
                circuit.update(dt);
                outputs[X_OUTPUT].setVoltage(circuit.vx());
                outputs[Y_OUTPUT].setVoltage(circuit.vy());
                outputs[Z_OUTPUT].setVoltage(circuit.vz());

                Tricorder::Message& msg = *static_cast<Tricorder::Message*>(rightExpander.producerMessage);
                msg.x = circuit.vx();
                msg.y = circuit.vy();
                msg.z = circuit.vz();
                rightExpander.requestMessageFlip();
            }
        };


        struct FrolicWidget : SapphireReloadableModuleWidget
        {
            explicit FrolicWidget(FrolicModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/frolic.svg"))
            {
                setModule(module);

                addSapphireOutput(X_OUTPUT, "x_output");
                addSapphireOutput(Y_OUTPUT, "y_output");
                addSapphireOutput(Z_OUTPUT, "z_output");

                addKnob(SPEED_KNOB_PARAM, "speed_knob");
                addKnob(CHAOS_KNOB_PARAM, "chaos_knob");

                // Load the SVG and place all controls at their correct coordinates.
                reloadPanel();
            }
        };
    }
}


Model* modelFrolic = createModel<Sapphire::Frolic::FrolicModule, Sapphire::Frolic::FrolicWidget>("Frolic");
