#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_simd.hpp"

// Sapphire Pivot for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Pivot
    {
        enum ParamId
        {
            TWIST_PARAM,
            TWIST_ATTEN,

            PARAMS_LEN
        };

        enum InputId
        {
            A_INPUT,
            TWIST_INPUT,

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

        const float MIN_TWIST = -3;
        const float MAX_TWIST = +3;

        struct PivotModule : SapphireModule
        {
            PivotModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(A_INPUT, "A");
                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");
                configOutput(C_OUTPUT, "Polyphonic (X, Y, Z)");

                configParam(TWIST_PARAM, MIN_TWIST, MAX_TWIST, 0, "Twist");
                configParam(TWIST_ATTEN, -1, +1, 0, "Twist attenuverter", "%", 0, 100);
                configInput(TWIST_INPUT, "Twist CV");

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
                PhysicsVector inVec{ax, ay, az, 0};

                float twist = getControlValue(TWIST_PARAM, TWIST_ATTEN, TWIST_INPUT, MIN_TWIST, MAX_TWIST);
                RotationMatrix rot = PivotAxes(twist);

                float cx = setFlippableOutputVoltage(X_OUTPUT, Dot(inVec, rot.xAxis));
                float cy = setFlippableOutputVoltage(Y_OUTPUT, Dot(inVec, rot.yAxis));
                float cz = setFlippableOutputVoltage(Z_OUTPUT, Dot(inVec, rot.zAxis));

                c.setChannels(3);
                c.setVoltage(cx, 0);
                c.setVoltage(cy, 1);
                c.setVoltage(cz, 2);

                sendVector(cx, cy, cz, false);
            }
        };


        struct PivotWidget : SapphireReloadableModuleWidget
        {
            PivotModule* pivotModule{};

            explicit PivotWidget(PivotModule* module)
                : SapphireReloadableModuleWidget("pivot", asset::plugin(pluginInstance, "res/pivot.svg"))
                , pivotModule(module)
            {
                setModule(module);
                addSapphireInput(A_INPUT, "a_input");
                addSapphireOutput(C_OUTPUT, "c_output");
                addFlippableOutputPort(X_OUTPUT, "x_output", module);
                addFlippableOutputPort(Y_OUTPUT, "y_output", module);
                addFlippableOutputPort(Z_OUTPUT, "z_output", module);
                addSapphireControlGroup("twist", TWIST_PARAM, TWIST_ATTEN, TWIST_INPUT);
            }

            void draw(const DrawArgs& args) override
            {
                SapphireReloadableModuleWidget::draw(args);
                float xcol = 1.7;
                drawFlipIndicator(args, X_OUTPUT, xcol,  88.0);
                drawFlipIndicator(args, Y_OUTPUT, xcol,  97.0);
                drawFlipIndicator(args, Z_OUTPUT, xcol, 106.0);
            }

            void drawFlipIndicator(const DrawArgs& args, int outputId, float x, float y)
            {
                if (pivotModule && pivotModule->getVoltageFlipEnabled(outputId))
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


Model* modelSapphirePivot = createSapphireModel<Sapphire::Pivot::PivotModule, Sapphire::Pivot::PivotWidget>(
    "Pivot",
    Sapphire::VectorRole::Sender
);
