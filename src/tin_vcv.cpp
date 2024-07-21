#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

// Tin (tricorder input module) for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace TricorderInput
    {
        enum ParamId
        {
            LEVEL_PARAM,
            LEVEL_ATTEN,

            PARAMS_LEN
        };

        enum InputId
        {
            X_INPUT,
            Y_INPUT,
            Z_INPUT,
            CLEAR_TRIGGER_INPUT,
            POLY_INPUT,
            LEVEL_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct TinModule : SapphireModule
        {
            GateTriggerReceiver resetTrigger;

            TinModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");
                configInput(POLY_INPUT, "Polyphonic (X, Y, Z)");
                configInput(CLEAR_TRIGGER_INPUT, "Clear display trigger");

                configParam(LEVEL_PARAM, 0, 2, 1, "Level", " dB", -10, 80);
                configParam(LEVEL_ATTEN, -1, +1, 0, "Level attenuverter", "%", 0, 100);
                configInput(LEVEL_INPUT, "Level CV");

                initialize();
            }

            void initialize()
            {
                resetTrigger.initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                // We allow input from the monophonic X, Y, and Z inputs.
                // We also allow input from the polyphonic P input.
                // To make either/both work, add voltages together.

                float x = inputs[X_INPUT].getVoltageSum();
                float y = inputs[Y_INPUT].getVoltageSum();
                float z = inputs[Z_INPUT].getVoltageSum();

                int nc = inputs[POLY_INPUT].channels;
                if (0 < nc) x += inputs[POLY_INPUT].getVoltage(0);
                if (1 < nc) y += inputs[POLY_INPUT].getVoltage(1);
                if (2 < nc) z += inputs[POLY_INPUT].getVoltage(2);

                float slider = getControlValue(LEVEL_PARAM, LEVEL_ATTEN, LEVEL_INPUT, 0, +2);
                float slider2 = slider * slider;
                float gain = slider2 * slider2;     // gain = slider^4
                x *= gain;
                y *= gain;
                z *= gain;

                bool reset = resetTrigger.updateTrigger(inputs[CLEAR_TRIGGER_INPUT].getVoltageSum());
                sendVector(x, y, z, reset);
            }
        };


        struct TinWidget : SapphireReloadableModuleWidget
        {
            explicit TinWidget(TinModule *module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tin.svg"))
            {
                setModule(module);
                addSapphireInput(X_INPUT, "x_input");
                addSapphireInput(Y_INPUT, "y_input");
                addSapphireInput(Z_INPUT, "z_input");
                addSapphireInput(POLY_INPUT, "p_input");
                addSapphireInput(CLEAR_TRIGGER_INPUT, "clear_trigger_input");
                addKnob(LEVEL_PARAM, "level_knob");
                addSapphireAttenuverter(LEVEL_ATTEN, "level_atten");
                addSapphireInput(LEVEL_INPUT, "level_cv");
                reloadPanel();      // Load the SVG and place all controls at their correct coordinates.
            }
        };
    }
}

Model* modelTin = createSapphireModel<Sapphire::TricorderInput::TinModule, Sapphire::TricorderInput::TinWidget>(
    "Tin",
    Sapphire::VectorRole::Sender
);
