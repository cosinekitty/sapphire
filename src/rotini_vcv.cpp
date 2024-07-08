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

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct RotiniModule : SapphireModule
        {
            RotiniModule()
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
                float cx = 0;
                float cy = 0;
                float cz = 0;
                vectorSender.sendVector(cx, cy, cz, false);
            }
        };


        struct RotiniWidget : SapphireReloadableModuleWidget
        {
            explicit RotiniWidget(RotiniModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/rotini.svg"))
            {
                setModule(module);
                addSapphireInput(A_INPUT, "a_input");
                addSapphireInput(B_INPUT, "b_input");
                addSapphireOutput(C_OUTPUT, "c_output");
                reloadPanel();
            }
        };
    }
}


Model* modelRotini = createSapphireModel<Sapphire::Rotini::RotiniModule, Sapphire::Rotini::RotiniWidget>(
    "Rotini",
    Sapphire::VectorRole::Sender
);
