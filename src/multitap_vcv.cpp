#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        namespace InLoop
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

            struct Mod : SapphireModule
            {
                Mod()
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

            struct Wid : SapphireWidget
            {
                Mod* inLoopModule{};

                explicit Wid(Mod* module)
                    : SapphireWidget("inloop", asset::plugin(pluginInstance, "res/inloop.svg"))
                    , inLoopModule(module)
                {
                    setModule(module);
                    auto toggle = createLightParamCentered<VCVLightBezelLatch<>>(Vec{}, module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addSapphireParam(toggle, "insert_button");
                }
            };
        }

        namespace Loop
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

            struct Mod : SapphireModule
            {
                Mod()
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

            struct Wid : SapphireWidget
            {
                Mod* loopModule{};

                explicit Wid(Mod* module)
                    : SapphireWidget("loop", asset::plugin(pluginInstance, "res/loop.svg"))
                    , loopModule(module)
                {
                    setModule(module);
                    auto toggle = createLightParamCentered<VCVLightBezelLatch<>>(Vec{}, module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
                    addSapphireParam(toggle, "insert_button");
                }
            };
        }

        namespace OutLoop
        {
            enum ParamId
            {
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
                LIGHTS_LEN
            };

            struct Mod : SapphireModule
            {
                Mod()
                    : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
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
                }
            };

            struct Wid : SapphireWidget
            {
                Mod* outLoopModule{};

                explicit Wid(Mod* module)
                    : SapphireWidget("outloop", asset::plugin(pluginInstance, "res/outloop.svg"))
                    , outLoopModule(module)
                {
                    setModule(module);
                }
            };
        }
    }
}


Model* modelSapphireInLoop = createSapphireModel<Sapphire::MultiTap::InLoop::Mod, Sapphire::MultiTap::InLoop::Wid>(
    "InLoop",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireLoop = createSapphireModel<Sapphire::MultiTap::Loop::Mod, Sapphire::MultiTap::Loop::Wid>(
    "Loop",
    Sapphire::ExpanderRole::MultiTap
);

Model* modelSapphireOutLoop = createSapphireModel<Sapphire::MultiTap::OutLoop::Mod, Sapphire::MultiTap::OutLoop::Wid>(
    "OutLoop",
    Sapphire::ExpanderRole::MultiTap
);
