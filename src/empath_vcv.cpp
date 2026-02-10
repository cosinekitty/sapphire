#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Empath
    {
        inline bool IsInput(const Module* module)
        {
            return IsModelType(module, modelSapphireEmpathInput);
        }

        inline bool IsFilter(const Module* module)
        {
            return IsModelType(module, modelSapphireEmpathFilter);
        }

        inline bool IsOutput(const Module* module)
        {
            return IsModelType(module, modelSapphireEmpathOutput);
        }

        inline bool IsFilterReceiver(const Module* module)
        {
            return IsFilter(module) || IsOutput(module);
        }


        struct EmpathModule : SapphireModule
        {
            int chainIndex = -1;
            float pendingMoveX{};
            int pendingMoveStepCount{};

            explicit EmpathModule(std::size_t nParams, std::size_t nOutputPorts)
                : SapphireModule(nParams, nOutputPorts)
                {}

            void beginMoveChain(float x)
            {
                pendingMoveX = x;
                pendingMoveStepCount = 2;
            }
        };


        struct EmpathWidget : SapphireWidget
        {
            explicit EmpathWidget(const std::string& moduleCode, const std::string& panelSvgFileName)
                : SapphireWidget(moduleCode, panelSvgFileName)
                {}

            EmpathModule* filterReceiverWithinRange()
            {
                if (module == nullptr)
                    return nullptr;

                if (Module* right = module->rightExpander.module)
                {
                    if (IsFilterReceiver(right))
                        return dynamic_cast<EmpathModule*>(right);

                    return nullptr;
                }

                // There is no module immediately to the right.
                // Search all modules in the Rack for anything to the right of this module.
                // Pick the module closest to this one, but only if it is within the width
                // we need for the hypothetical filter module we are about to insert.
                const int hpEchoTap = hpDistance(PanelWidth("empath_filter"));
                assert(hpEchoTap > 0);
                if (ModuleWidget* closestWidget = FindWidgetClosestOnRight(this, hpEchoTap))
                    if (IsFilterReceiver(closestWidget->module))
                        return dynamic_cast<EmpathModule*>(closestWidget->module);

                return nullptr;
            }

            void addExpanderInsertButton(int paramId);

            void insertExpander()
            {
                if (module == nullptr)
                    return;

                // If in the middle of a chain, insert another filter.
                // Otherwise terminate the chain with an output module.

                Module* right = filterReceiverWithinRange();
                Model* model = right ? modelSapphireEmpathFilter : modelSapphireEmpathOutput;

                // Erase any obsolete chain indices already in the remaining modules.
                // This prevents them briefly flashing on the screen before being replaced.
                for (Module* m = right; IsFilter(m); m = m->rightExpander.module)
                    if (auto em = dynamic_cast<EmpathModule*>(m))
                        em->chainIndex = -1;

                // Create the expander module.
                AddExpander(model, this, ExpanderDirection::Right, true);
            }

            void removeExpander()
            {
                if (module == nullptr)
                    return;

                // Hand responsibility for moving the rest of the chain to the next module in the chain.
                if (auto nextModule = dynamic_cast<EmpathModule*>(module->rightExpander.module))
                    nextModule->beginMoveChain(box.pos.x);

                // *** DANGER DANGER DANGER ***
                // The following code deletes `this`.
                // Can't do anything more to this object!
                removeAction();
            }
        };


        using insert_button_base_t = app::SvgSwitch;
        struct InsertButton : insert_button_base_t
        {
            EmpathWidget* empathWidget{};

            explicit InsertButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/right_extender_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };


        using remove_button_base_t = app::SvgSwitch;
        struct RemoveButton : remove_button_base_t
        {
            //**********************************************************************************
            // WARNING: DO NOT try to make RemoveButton derive from SapphireTinyActionButton.
            // It will crash because the remove button deletes its own object!
            // This requires extremely careful coding to avoid corrupting memory.
            // Put simply: it works, so don't fix it!
            // Besides which, this button does not change its representation on the screen,
            // so there is no need to "blink" it in the first place.
            //**********************************************************************************

            EmpathWidget* empathWidget{};

            explicit RemoveButton()
            {
                momentary = true;
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/remove_button.svg")));
            }

            void onButton(const event::Button& e) override;
        };


        namespace Input
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
                LIGHTS_LEN
            };

            struct InputModule : EmpathModule
            {
                explicit InputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }
            };

            struct InputWidget : EmpathWidget
            {
                InputModule* inputModule{};

                explicit InputWidget(InputModule* module)
                    : EmpathWidget("empath_input", asset::plugin(pluginInstance, "res/empath_input.svg"))
                    , inputModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(INSERT_BUTTON_PARAM);
                }
            };
        };

        //----------------------------------------------------------------------------

        namespace Filter
        {
            enum ParamId
            {
                INSERT_BUTTON_PARAM,
                REMOVE_BUTTON_PARAM,
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

            struct FilterModule : EmpathModule
            {
                explicit FilterModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }
            };

            struct FilterWidget : EmpathWidget
            {
                FilterModule* filterModule{};

                explicit FilterWidget(FilterModule* module)
                    : EmpathWidget("empath_filter", asset::plugin(pluginInstance, "res/empath_filter.svg"))
                    , filterModule(module)
                {
                    setModule(module);
                    addExpanderInsertButton(INSERT_BUTTON_PARAM);
                    addExpanderRemoveButton(REMOVE_BUTTON_PARAM);
                }

                void addExpanderRemoveButton(int paramId)
                {
                    auto button = createParamCentered<RemoveButton>(Vec{}, filterModule, paramId);
                    button->empathWidget = this;
                    addSapphireParam(button, "remove_button");
                }
            };
        }

        //----------------------------------------------------------------------------

        namespace Output
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

            struct OutputModule : EmpathModule
            {
                explicit OutputModule()
                    : EmpathModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }
            };

            struct OutputWidget : EmpathWidget
            {
                OutputModule* outputModule{};

                explicit OutputWidget(OutputModule* module)
                    : EmpathWidget("empath_output", asset::plugin(pluginInstance, "res/empath_output.svg"))
                    , outputModule(module)
                {
                    setModule(module);
                }
            };
        }

        void InsertButton::onButton(const event::Button& e)
        {
            insert_button_base_t::onButton(e);
            if (empathWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    empathWidget->insertExpander();
            }
        }


        void RemoveButton::onButton(const event::Button& e)
        {
            remove_button_base_t::onButton(e);
            if (empathWidget)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                    empathWidget->removeExpander();
            }
        }

        void EmpathWidget::addExpanderInsertButton(int paramId)
        {
            auto button = createParamCentered<InsertButton>(Vec{}, module, paramId);
            button->empathWidget = this;
            addSapphireParam(button, "insert_button");
        }
    }
}

Model* modelSapphireEmpathInput = createSapphireModel<Sapphire::Empath::Input::InputModule, Sapphire::Empath::Input::InputWidget>(
    "Empath",
    Sapphire::ExpanderRole::Empath
);

Model* modelSapphireEmpathFilter = createSapphireModel<Sapphire::Empath::Filter::FilterModule, Sapphire::Empath::Filter::FilterWidget>(
    "EmpathFilter",
    Sapphire::ExpanderRole::Empath
);

Model* modelSapphireEmpathOutput = createSapphireModel<Sapphire::Empath::Output::OutputModule, Sapphire::Empath::Output::OutputWidget>(
    "EmpathOutput",
    Sapphire::ExpanderRole::Empath
);
