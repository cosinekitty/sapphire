#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "tricorder.hpp"

// Tin (tricorder input module) for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace TricorderInput
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
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct TinModule : Module
        {
            Tricorder::Communicator communicator;

            TinModule()
                : communicator(*this)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");
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
                float x = inputs[X_INPUT].getVoltageSum();
                float y = inputs[Y_INPUT].getVoltageSum();
                float z = inputs[Z_INPUT].getVoltageSum();
                communicator.sendVector(x, y, z);
            }

            json_t* dataToJson() override
            {
                // No state to save yet, but keep the option open for later.
                // For now, create an empty object "{}".
                return json_object();
            }

            void dataFromJson(json_t* root) override
            {
                // No state to load yet
            }
        };

        struct TinWidget : SapphireReloadableModuleWidget
        {
            explicit TinWidget(TinModule *module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tin.svg"))
            {
                setModule(module);
                addSapphireInput(X_INPUT, "x_input");
                addSapphireInput(Y_INPUT, "y_input");
                addSapphireInput(Z_INPUT, "z_input");
                reloadPanel();      // Load the SVG and place all controls at their correct coordinates.
            }
        };
    }
}

Model* modelTin = createModel<Sapphire::TricorderInput::TinModule, Sapphire::TricorderInput::TinWidget>("Tin");
