#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        struct InLoopModule : SapphireModule
        {
            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                PARAMS_LEN
            };

            enum InputId
            {
                INPUTS_LEN
            };

            enum OutputId
            {
                OUTPUTS_LEN
            };

            enum LightId
            {
                INSERT_BUTTON_LIGHT,
                LIGHTS_LEN
            };

            InLoopModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configButton(INSERT_BUTTON_PARAM, "Insert loop expander");
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
            }
        };


        struct InLoopWidget : SapphireWidget
        {
            InLoopModule* inLoopModule{};

            explicit InLoopWidget(InLoopModule* module)
                : SapphireWidget("inloop", asset::plugin(pluginInstance, "res/inloop.svg"))
                , inLoopModule(module)
            {
                setModule(module);
                auto toggle = createLightParamCentered<VCVLightBezelLatch<>>(Vec{}, module, InLoopModule::INSERT_BUTTON_PARAM, InLoopModule::INSERT_BUTTON_LIGHT);
                addSapphireParam(toggle, "insert_button");
            }
        };
    }
}


Model* modelSapphireInLoop = createSapphireModel<Sapphire::MultiTap::InLoopModule, Sapphire::MultiTap::InLoopWidget>(
    "InLoop",
    Sapphire::ExpanderRole::MultiTap
);
