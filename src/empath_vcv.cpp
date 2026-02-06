#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Empath
    {
        namespace Input
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

            struct InputModule : SapphireModule
            {
                explicit InputModule()
                    : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }
            };

            struct InputWidget : SapphireWidget
            {
                InputModule* inputModule{};

                explicit InputWidget(InputModule* module)
                    : SapphireWidget("empath_input", asset::plugin(pluginInstance, "res/empath_input.svg"))
                    , inputModule(module)
                {
                    setModule(module);
                }
            };
        };

        //----------------------------------------------------------------------------

        namespace Filter
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

            struct FilterModule : SapphireModule
            {
                explicit FilterModule()
                    : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }
            };

            struct FilterWidget : SapphireWidget
            {
                FilterModule* filterModule{};

                explicit FilterWidget(FilterModule* module)
                    : SapphireWidget("empath_filter", asset::plugin(pluginInstance, "res/empath_filter.svg"))
                    , filterModule(module)
                {
                    setModule(module);
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

            struct OutputModule : SapphireModule
            {
                explicit OutputModule()
                    : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                {
                    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                }
            };

            struct OutputWidget : SapphireWidget
            {
                OutputModule* outputModule{};

                explicit OutputWidget(OutputModule* module)
                    : SapphireWidget("empath_output", asset::plugin(pluginInstance, "res/empath_output.svg"))
                    , outputModule(module)
                {
                    setModule(module);
                }
            };
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
