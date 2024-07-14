#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

// Sapphire Pivot for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Pivot
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            A_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            C_OUTPUT,
            X_OUTPUT,
            Y_OUTPUT,
            Z_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct PivotModule : SapphireModule
        {
            PivotModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

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
                auto& a = inputs[A_INPUT];
                auto& c = outputs[C_OUTPUT];

                float ax = a.getVoltage(0);
                float ay = a.getVoltage(1);
                float az = a.getVoltage(2);

                //const float denom = 5;      // normalize so that 5V = unity

                float cx = ay;      // FIXFIXFIX
                float cy = az;
                float cz = ax;

                c.setChannels(3);
                c.setVoltage(cx, 0);
                c.setVoltage(cy, 1);
                c.setVoltage(cz, 2);

                outputs[X_OUTPUT].setVoltage(cx);
                outputs[Y_OUTPUT].setVoltage(cy);
                outputs[Z_OUTPUT].setVoltage(cz);

                vectorSender.sendVector(cx, cy, cz, false);
            }
        };


        struct PivotWidget : SapphireReloadableModuleWidget
        {
            explicit PivotWidget(PivotModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/pivot.svg"))
            {
                setModule(module);
                addSapphireInput(A_INPUT, "a_input");
                addSapphireOutput(C_OUTPUT, "c_output");
                addSapphireOutput(X_OUTPUT, "x_output");
                addSapphireOutput(Y_OUTPUT, "y_output");
                addSapphireOutput(Z_OUTPUT, "z_output");
                reloadPanel();
            }
        };
    }
}


Model* modelPivot = createSapphireModel<Sapphire::Pivot::PivotModule, Sapphire::Pivot::PivotWidget>(
    "Pivot",
    Sapphire::VectorRole::Sender
);
