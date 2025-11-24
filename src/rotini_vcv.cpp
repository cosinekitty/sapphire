#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

// Sapphire Rotini for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Rotini
    {
        enum ParamId
        {
            ADD_TRICORDER_BUTTON_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            A_INPUT,
            B_INPUT,

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


        struct RotiniModule : SapphireModule
        {
            RotiniModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(A_INPUT, "A");
                configInput(B_INPUT, "B");
                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");
                configOutput(C_OUTPUT, "Polyphonic (X, Y, Z)");
                configButton(ADD_TRICORDER_BUTTON_PARAM, "Insert Tricorder");
                initialize();
            }

            void initialize()
            {
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                auto& a = inputs.at(A_INPUT);
                auto& b = inputs.at(B_INPUT);
                auto& c = outputs.at(C_OUTPUT);

                float ax = a.getVoltage(0);
                float ay = a.getVoltage(1);
                float az = a.getVoltage(2);

                float bx = b.getVoltage(0);
                float by = b.getVoltage(1);
                float bz = b.getVoltage(2);

                const float denom = 5;      // normalize so that 5V = unity

                float rx = (ay*bz - az*by) / denom;
                float ry = (az*bx - ax*bz) / denom;
                float rz = (ax*by - ay*bx) / denom;

                float cx = setFlippableOutputVoltage(X_OUTPUT, rx);
                float cy = setFlippableOutputVoltage(Y_OUTPUT, ry);
                float cz = setFlippableOutputVoltage(Z_OUTPUT, rz);

                c.setChannels(3);
                c.setVoltage(cx, 0);
                c.setVoltage(cy, 1);
                c.setVoltage(cz, 2);

                sendVector(cx, cy, cz, false);
            }
        };


        struct RotiniWidget : SapphireWidget
        {
            RotiniModule* rotiniModule{};

            explicit RotiniWidget(RotiniModule* module)
                : SapphireWidget("rotini", asset::plugin(pluginInstance, "res/rotini.svg"))
                , rotiniModule(module)
            {
                setModule(module);
                addSapphireInput(A_INPUT, "a_input");
                addSapphireInput(B_INPUT, "b_input");
                addSapphireOutput(C_OUTPUT, "c_output");
                addFlippableOutputPort(X_OUTPUT, "x_output", module);
                addFlippableOutputPort(Y_OUTPUT, "y_output", module);
                addFlippableOutputPort(Z_OUTPUT, "z_output", module);
                addInsertTricorderButton(ADD_TRICORDER_BUTTON_PARAM);
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);
                float xcol = 1.7;
                drawFlipIndicator(args, X_OUTPUT, xcol,  88.0);
                drawFlipIndicator(args, Y_OUTPUT, xcol,  97.0);
                drawFlipIndicator(args, Z_OUTPUT, xcol, 106.0);
            }

            void drawFlipIndicator(const DrawArgs& args, int outputId, float x, float y)
            {
                if (rotiniModule && rotiniModule->getVoltageFlipEnabled(outputId))
                {
                    const float dx = 0.75;
                    nvgBeginPath(args.vg);
                    nvgStrokeColor(args.vg, SCHEME_BLACK);
                    nvgStrokeWidth(args.vg, 1.0);
                    nvgMoveTo(args.vg, mm2px(x-dx), mm2px(y));
                    nvgLineTo(args.vg, mm2px(x+dx), mm2px(y));
                    nvgStroke(args.vg);
                }
            }
        };
    }
}


Model* modelSapphireRotini = createSapphireModel<Sapphire::Rotini::RotiniModule, Sapphire::Rotini::RotiniWidget>(
    "Rotini",
    Sapphire::ExpanderRole::VectorSender
);
