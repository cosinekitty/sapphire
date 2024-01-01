#include "plugin.hpp"
#include "sapphire_widget.hpp"

// Nucleus for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Nucleus
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

            INPUTS_LEN
        };

        enum OutputId
        {
            X1_OUTPUT,
            Y1_OUTPUT,
            Z1_OUTPUT,

            X2_OUTPUT,
            Y2_OUTPUT,
            Z2_OUTPUT,

            X3_OUTPUT,
            Y3_OUTPUT,
            Z3_OUTPUT,

            X4_OUTPUT,
            Y4_OUTPUT,
            Z4_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct NucleusModule : Module
        {
            NucleusModule()
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");

                configOutput(X1_OUTPUT, "X1");
                configOutput(Y1_OUTPUT, "Y1");
                configOutput(Z1_OUTPUT, "Z1");
                configOutput(X2_OUTPUT, "X2");
                configOutput(Y2_OUTPUT, "Y2");
                configOutput(Z2_OUTPUT, "Z2");
                configOutput(X3_OUTPUT, "X3");
                configOutput(Y3_OUTPUT, "Y3");
                configOutput(Z3_OUTPUT, "Z3");
                configOutput(X4_OUTPUT, "X4");
                configOutput(Y4_OUTPUT, "Y4");
                configOutput(Z4_OUTPUT, "Z4");
            }
        };

        struct NucleusWidget : SapphireReloadableModuleWidget
        {
            explicit NucleusWidget(NucleusModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/nucleus.svg"))
            {
                setModule(module);

                addSapphireInput(X_INPUT, "x_input");
                addSapphireInput(Y_INPUT, "y_input");
                addSapphireInput(Z_INPUT, "z_input");

                addSapphireOutput(X1_OUTPUT, "x1_output");
                addSapphireOutput(Y1_OUTPUT, "y1_output");
                addSapphireOutput(Z1_OUTPUT, "z1_output");
                addSapphireOutput(X2_OUTPUT, "x2_output");
                addSapphireOutput(Y2_OUTPUT, "y2_output");
                addSapphireOutput(Z2_OUTPUT, "z2_output");
                addSapphireOutput(X3_OUTPUT, "x3_output");
                addSapphireOutput(Y3_OUTPUT, "y3_output");
                addSapphireOutput(Z3_OUTPUT, "z3_output");
                addSapphireOutput(X4_OUTPUT, "x4_output");
                addSapphireOutput(Y4_OUTPUT, "y4_output");
                addSapphireOutput(Z4_OUTPUT, "z4_output");

                reloadPanel();
            }
        };
    }
}


Model* modelNucleus = createModel<Sapphire::Nucleus::NucleusModule, Sapphire::Nucleus::NucleusWidget>("Nucleus");
