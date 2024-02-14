#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "tricorder.hpp"

// Tin (tricorder input module) for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace TricorderInput
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            X_INPUT,
            Y_INPUT,
            Z_INPUT,
            CLEAR_TRIGGER_INPUT,
            POLY_INPUT,

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

        struct TinModule : Module
        {
            Tricorder::VectorSender vectorSender;
            GateTriggerReceiver resetTrigger;

            TinModule()
                : vectorSender(*this)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");
                configInput(POLY_INPUT, "Polyphonic (X, Y, Z)");
                configInput(CLEAR_TRIGGER_INPUT, "Clear display trigger");
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

                bool reset = resetTrigger.updateTrigger(inputs[CLEAR_TRIGGER_INPUT].getVoltageSum());
                vectorSender.sendVector(x, y, z, reset);
            }

            json_t* dataToJson() override
            {
                // No state to save yet, but keep the option open for later.
                // For now, create an empty object "{}".
                return json_object();
            }

            void dataFromJson(json_t* root) override
            {
                // No state to load yet
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
                reloadPanel();      // Load the SVG and place all controls at their correct coordinates.
            }
        };
    }
}

Model* modelTin = createModel<Sapphire::TricorderInput::TinModule, Sapphire::TricorderInput::TinWidget>("Tin");
