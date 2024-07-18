#pragma once
#include "sapphire_vcvrack.hpp"
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
            bool turboMode = false;

            ChaosModule()
                : SapphireModule(PARAMS_LEN)
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
                turboMode = false;
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t *root = SapphireModule::dataToJson();
                json_object_set_new(root, "turboMode", json_boolean(turboMode));
                json_object_set_new(root, "chaosMode", json_integer(circuit.getMode()));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);

                json_t* flag = json_object_get(root, "turboMode");
                turboMode = json_is_true(flag);

                json_t* mode = json_object_get(root, "chaosMode");
                if (json_is_integer(mode))
                    circuit.setMode(json_integer_value(mode));
            }

            void process(const ProcessArgs& args) override
            {
                float chaos = getControlValue(CHAOS_KNOB_PARAM, CHAOS_ATTEN, CHAOS_CV_INPUT, -1, +1);
                circuit.setKnob(chaos);
                float speed = getControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN, SPEED_CV_INPUT, -7, +7);
                if (turboMode)
                    speed += 5;
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
            module_t *chaosModule;

            ChaosWidget(module_t* module, const char *panelSvgFileName)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, panelSvgFileName))
                , chaosModule(module)
            {
                setModule(module);

                addSapphireOutput(X_OUTPUT, "x_output");
                addSapphireOutput(Y_OUTPUT, "y_output");
                addSapphireOutput(Z_OUTPUT, "z_output");
                addSapphireOutput(POLY_OUTPUT, "p_output");

                addKnob(SPEED_KNOB_PARAM, "speed_knob");
                addKnob(CHAOS_KNOB_PARAM, "chaos_knob");

                addSapphireAttenuverter(SPEED_ATTEN, "speed_atten");
                addSapphireAttenuverter(CHAOS_ATTEN, "chaos_atten");

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(CHAOS_CV_INPUT, "chaos_cv");

                // Load the SVG and place all controls at their correct coordinates.
                reloadPanel();
            }

            void appendContextMenu(Menu* menu) override
            {
                if (chaosModule == nullptr)
                    return;

                menu->addChild(new MenuSeparator);

                menu->addChild(createBoolMenuItem(
                    "Turbo mode: +5 speed bonus (WARNING: uses more CPU)",
                    "",
                    [=]()
                    {
                        return chaosModule->turboMode;
                    },
                    [=](bool state)
                    {
                        chaosModule->turboMode = state;
                    }
                ));

                // If this chaos module supports multiple parameter modes,
                // enumerate them now as menu options.
                const int numModes = chaosModule->circuit.getModeCount();
                if (numModes > 1)
                {
                    menu->addChild(new MenuSeparator);

                    std::vector<std::string> labels;
                    for (int mode = 0; mode < numModes; ++mode)
                        labels.push_back(chaosModule->circuit.getModeName(mode));

                    menu->addChild(createIndexSubmenuItem(
                        "Chaos mode",
                        labels,
                        [=]() { return chaosModule->circuit.getMode(); },
                        [=](size_t mode) { chaosModule->circuit.setMode(mode); }
                    ));
                }
            }
        };
    }
}
