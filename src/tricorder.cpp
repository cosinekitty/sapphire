#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "tricorder.hpp"

// Tricorder for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Tricorder
    {
        struct TricorderWidget;

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

        struct TricorderModule : Module
        {
            TricorderModule()
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

            static bool isCompatibleModule(const Module *module)
            {
                if (module == nullptr)
                    return false;

                if (module->model == modelFrolic)
                    return true;

                return false;
            }

            const TricorderMessage* inboundMessage() const
            {
                if (isCompatibleModule(leftExpander.module))
                    return static_cast<const TricorderMessage *>(leftExpander.module->rightExpander.consumerMessage);

                return nullptr;
            }

            void process(const ProcessArgs& args) override
            {
                // Is a compatible module connected to the left?
                // If so, receive a triplet of voltages from it and put them in the buffer.
                const TricorderMessage *msg = inboundMessage();
                if (msg == nullptr)
                {
                    // FIXFIXFIX - clear the display if it has stuff on it
                }
                else
                {
                    // FIXFIXFIX - buffer the voltages
                }
            }
        };


        struct TricorderDisplay : LedDisplay
        {
            TricorderModule* module;
            TricorderWidget* parent;

            TricorderDisplay(TricorderModule* _module, TricorderWidget* _parent)
                : module(_module)
                , parent(_parent)
            {
                box.pos = mm2px(Vec(8.0f, 12.0f));
                box.size = mm2px(Vec(110.0f, 105.0f));
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                if (layer != 1)
                    return;

                drawBackground(args);

                if (module == nullptr)
                    return;
            }

            void drawBackground(const DrawArgs& args)
            {
            }
        };


        struct TricorderWidget : SapphireReloadableModuleWidget
        {
            explicit TricorderWidget(TricorderModule *module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tricorder.svg"))
            {
                setModule(module);

                // Load the SVG and place all controls at their correct coordinates.
                reloadPanel();

                addChild(new TricorderDisplay(module, this));
            }
        };
    }
}

Model *modelTricorder = createModel<Sapphire::Tricorder::TricorderModule, Sapphire::Tricorder::TricorderWidget>("Tricorder");
