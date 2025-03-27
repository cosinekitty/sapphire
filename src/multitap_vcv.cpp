#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace MultiTap
    {
        using insert_button_base_t = VCVLightBezel<WhiteLight>;

        struct LoopWidget;

        struct InsertButton : insert_button_base_t
        {
            LoopWidget* loopWidget{};

            void onButton(const event::Button& e) override;
        };

        struct LoopModule : SapphireModule
        {
            explicit LoopModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
                {}
        };

        struct LoopWidget : SapphireWidget
        {
            explicit LoopWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            void addExpanderInsertButton(LoopModule* loopModule, int paramId, int lightId)
            {
                auto button = createLightParamCentered<InsertButton>(Vec{}, loopModule, paramId, lightId);
                button->loopWidget = this;
                addSapphireParam(button, "insert_button");
            }
        };


        void InsertButton::onButton(const event::Button& e)
        {
            insert_button_base_t::onButton(e);
            if (loopWidget != nullptr)
            {
                if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    AddExpander(modelSapphireLoop, loopWidget, ExpanderDirection::Right);
            }
        }


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

            struct Mod : LoopModule
            {
                Mod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
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

            struct Wid : LoopWidget
            {
                Mod* inLoopModule{};

                explicit Wid(Mod* module)
                    : LoopWidget("inloop", asset::plugin(pluginInstance, "res/inloop.svg"))
                    , inLoopModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
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

            struct Mod : LoopModule
            {
                Mod()
                    : LoopModule(PARAMS_LEN, OUTPUTS_LEN)
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

            struct Wid : LoopWidget
            {
                Mod* loopModule{};

                explicit Wid(Mod* module)
                    : LoopWidget("loop", asset::plugin(pluginInstance, "res/loop.svg"))
                    , loopModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(module, INSERT_BUTTON_PARAM, INSERT_BUTTON_LIGHT);
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
