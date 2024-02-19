#pragma once
#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_simd.hpp"
#include "chaos.hpp"

// Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire
// Generic pattern for a Sapphire-style vector-output chaos module (Frolic & Glee).

namespace Sapphire
{
    namespace Chaos
    {
        enum ParamId
        {
            // Large knobs for manual parameter adjustment
            SPEED_KNOB_PARAM,
            CHAOS_KNOB_PARAM,

            // Attenuverter knobs
            SPEED_ATTEN,
            CHAOS_ATTEN,

            PARAMS_LEN
        };

        enum InputId
        {
            // Control group CV inputs
            SPEED_CV_INPUT,
            CHAOS_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            X_OUTPUT,
            Y_OUTPUT,
            Z_OUTPUT,
            POLY_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        template <typename circuit_t>
        struct ChaosModule : SapphireModule
        {
            circuit_t circuit;

            ChaosModule()
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");
                configOutput(POLY_OUTPUT, "Polyphonic (X, Y, Z)");

                configParam(SPEED_KNOB_PARAM, -7, +7, 0, "Speed");
                configParam(CHAOS_KNOB_PARAM, -1, +1, 0, "Chaos");

                configParam(SPEED_ATTEN, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(CHAOS_ATTEN, -1, 1, 0, "Chaos attenuverter", "%", 0, 100);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(CHAOS_CV_INPUT, "Chaos CV");

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
                float chaos = getControlValue(CHAOS_KNOB_PARAM, CHAOS_ATTEN, CHAOS_CV_INPUT, -1, +1);
                circuit.setKnob(chaos);
                float speed = getControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN, SPEED_CV_INPUT, -7, +7);
                double dt = args.sampleTime * std::pow(2.0f, speed);
                circuit.update(dt);
                outputs[X_OUTPUT].setVoltage(circuit.vx());
                outputs[Y_OUTPUT].setVoltage(circuit.vy());
                outputs[Z_OUTPUT].setVoltage(circuit.vz());
                outputs[POLY_OUTPUT].setChannels(3);
                outputs[POLY_OUTPUT].setVoltage(circuit.vx(), 0);
                outputs[POLY_OUTPUT].setVoltage(circuit.vy(), 1);
                outputs[POLY_OUTPUT].setVoltage(circuit.vz(), 2);
                vectorSender.sendVector(circuit.vx(), circuit.vy(), circuit.vz(), false);
            }
        };


        template <typename module_t>
        struct ChaosWidget : SapphireReloadableModuleWidget
        {
            ChaosWidget(module_t* module, const char *panelSvgFileName)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, panelSvgFileName))
            {
                setModule(module);

                addSapphireOutput(X_OUTPUT, "x_output");
                addSapphireOutput(Y_OUTPUT, "y_output");
                addSapphireOutput(Z_OUTPUT, "z_output");
                addSapphireOutput(POLY_OUTPUT, "p_output");

                addKnob(SPEED_KNOB_PARAM, "speed_knob");
                addKnob(CHAOS_KNOB_PARAM, "chaos_knob");

                addAttenuverter(SPEED_ATTEN, "speed_atten");
                addAttenuverter(CHAOS_ATTEN, "chaos_atten");

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(CHAOS_CV_INPUT, "chaos_cv");

                // Load the SVG and place all controls at their correct coordinates.
                reloadPanel();
            }
        };
    }
}
