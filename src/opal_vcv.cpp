#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Opal
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            POS_INPUT,
            NEG_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            CONTROL_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct OpalModule : SapphireModule
        {
            OpalModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(POS_INPUT, "Positive");
                configInput(NEG_INPUT, "Negative");
                configOutput(CONTROL_OUTPUT, "Control");
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
                // https://apmonitor.com/pdc/index.php/Main/ProportionalIntegralControl
            }
        };


        struct OpalWidget : SapphireWidget
        {
            OpalModule* opalModule{};

            explicit OpalWidget(OpalModule* module)
                : SapphireWidget("opal", asset::plugin(pluginInstance, "res/opal.svg"))
                , opalModule(module)
            {
                setModule(module);
                addSapphireInput(POS_INPUT, "pos_input");
                addSapphireInput(NEG_INPUT, "neg_input");
                addSapphireOutput(CONTROL_OUTPUT, "control_output");
            }
        };
    }
}


Model* modelSapphireOpal = createSapphireModel<Sapphire::Opal::OpalModule, Sapphire::Opal::OpalWidget>(
    "Opal",
    Sapphire::ExpanderRole::None
);
