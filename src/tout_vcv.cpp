#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

// Tout (tricorder output module) for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace TricorderOutput
    {
        enum ParamId
        {
            LEVEL_PARAM,
            LEVEL_ATTEN,

            PARAMS_LEN
        };

        enum InputId
        {
            LEVEL_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            X_OUTPUT,
            Y_OUTPUT,
            Z_OUTPUT,
            CLEAR_TRIGGER_OUTPUT,
            POLY_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct ToutModule : SapphireModule
        {
            TriggerSender triggerSender;

            ToutModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");
                configOutput(POLY_OUTPUT, "Polyphonic (X, Y, Z)");
                configOutput(CLEAR_TRIGGER_OUTPUT, "Clear display trigger");
                configParam(LEVEL_PARAM, 0, 2, 1, "Level", " dB", -10, 80);
                configParam(LEVEL_ATTEN, -1, +1, 0, "Level attenuverter", "%", 0, 100);
                configInput(LEVEL_INPUT, "Level CV");
                initialize();
            }

            void initialize()
            {
                triggerSender.initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                using namespace Sapphire::Tricorder;

                // Start with default values for (x, y, z).
                float x = 0;
                float y = 0;
                float z = 0;
                bool clear = false;

                // If a Tricoder-compatible vector sender exists to the left side,
                // receive a vector from it.
                Message* message = vectorReceiver.inboundVectorMessage();
                if (message != nullptr)
                {
                    x = message->x;
                    y = message->y;
                    z = message->z;
                    clear = message->isResetRequested();
                }

                // Adjust levels using the LEVEL control group.
                float slider = getControlValue(LEVEL_PARAM, LEVEL_ATTEN, LEVEL_INPUT, 0, +2);
                float slider2 = slider * slider;
                float gain = slider2 * slider2;     // gain = slider^4
                x *= gain;
                y *= gain;
                z *= gain;

                // Apply these values to the output ports.

                outputs[X_OUTPUT].setVoltage(x);
                outputs[Y_OUTPUT].setVoltage(y);
                outputs[Z_OUTPUT].setVoltage(z);

                outputs[POLY_OUTPUT].setChannels(3);
                outputs[POLY_OUTPUT].setVoltage(x, 0);
                outputs[POLY_OUTPUT].setVoltage(y, 1);
                outputs[POLY_OUTPUT].setVoltage(z, 2);

                float triggerVoltage = triggerSender.process(args.sampleTime, clear);
                outputs[CLEAR_TRIGGER_OUTPUT].setVoltage(triggerVoltage);

                // Mirror the level-adjusted input to any module on the right.
                sendVector(x, y, z, clear);
            }
        };

        struct ToutWidget : SapphireReloadableModuleWidget
        {
            explicit ToutWidget(ToutModule *module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tout.svg"))
            {
                setModule(module);
                addSapphireOutput(X_OUTPUT, "x_output");
                addSapphireOutput(Y_OUTPUT, "y_output");
                addSapphireOutput(Z_OUTPUT, "z_output");
                addSapphireOutput(POLY_OUTPUT, "p_output");
                addSapphireOutput(CLEAR_TRIGGER_OUTPUT, "clear_trigger_output");
                addKnob(LEVEL_PARAM, "level_knob");
                addSapphireAttenuverter(LEVEL_ATTEN, "level_atten");
                addSapphireInput(LEVEL_INPUT, "level_cv");
                reloadPanel();      // Load the SVG and place all controls at their correct coordinates.
            }
        };
    }
}

Model* modelSapphireTout = createSapphireModel<Sapphire::TricorderOutput::ToutModule, Sapphire::TricorderOutput::ToutWidget>(
    "Tout",
    Sapphire::VectorRole::SenderAndReceiver
);
