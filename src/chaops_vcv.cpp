#include "sapphire_chaos_module.hpp"

// Chaos Operators (Chaops)
// A left-expander for Sapphire chaos modules like Frolic, Glee, Lark.
// Adds extra functionality for controlling the internal behavior
// of the chaotic oscillator algorithms.

namespace Sapphire
{
    namespace ChaosOperators
    {
        enum ParamId
        {
            MEMORY_SELECT_PARAM,
            MEMORY_SELECT_ATTEN,
            STORE_BUTTON_PARAM,
            RECALL_BUTTON_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            MEMORY_SELECT_CV_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            OUTPUTS_LEN
        };

        enum LightId
        {
            STORE_BUTTON_LIGHT,
            RECALL_BUTTON_LIGHT,
            LIGHTS_LEN
        };

        struct ChaopsModule : SapphireModule
        {
            Sender sender;
            bool storeButtonPressed = false;
            bool recallButtonPressed = false;

            ChaopsModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                , sender(*this)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configParam(MEMORY_SELECT_PARAM, 0, MemoryCount-1, 0, "Memory select");
                paramQuantities[MEMORY_SELECT_PARAM]->snapEnabled = true;
                configParam(MEMORY_SELECT_ATTEN, -1, +1, 0, "Memory select attenuverter", "%", 0, 100);
                configInput(MEMORY_SELECT_CV_INPUT, "Memory select CV");
                configButton(STORE_BUTTON_PARAM, "Store");
                configButton(RECALL_BUTTON_PARAM, "Recall");
                initialize();
            }

            void initialize()
            {
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            int getMemoryIndex()
            {
                using namespace std;

                float v = getControlValue(MEMORY_SELECT_PARAM, MEMORY_SELECT_ATTEN, MEMORY_SELECT_CV_INPUT, 0, MemoryCount-1);
                // Use modular wraparound and snap to nearest integer in the allowed index range 0..15.
                // FIXFIXFIX - eliminate pre-clamping so modular wraparound is more useful.
                int index = max(0u, static_cast<unsigned>(round(v)) % MemoryCount);

                return index;
            }

            bool getStoreTrigger()
            {
                bool pressed = (params[STORE_BUTTON_PARAM].getValue() > 0);
                bool trigger = pressed && !storeButtonPressed;
                storeButtonPressed = pressed;
                return trigger;
            }

            bool getRecallTrigger()
            {
                bool pressed = (params[RECALL_BUTTON_PARAM].getValue() > 0);
                bool trigger = pressed && !recallButtonPressed;
                recallButtonPressed = pressed;
                return trigger;
            }

            void process(const ProcessArgs& args) override
            {
                if (sender.isReceiverConnectedOnRight())
                {
                    Message message;
                    message.memoryIndex = getMemoryIndex();
                    message.store = getStoreTrigger();
                    message.recall = getRecallTrigger();
                    sender.send(message);
                }
            }
        };


        struct ChaopsWidget : SapphireWidget
        {
            ChaopsModule* chaopsModule;

            explicit ChaopsWidget(ChaopsModule* module)
                : SapphireWidget("chaops", asset::plugin(pluginInstance, "res/chaops.svg"))
                , chaopsModule(module)
            {
                setModule(module);
                addSapphireFlatControlGroup("memsel", MEMORY_SELECT_PARAM, MEMORY_SELECT_ATTEN, MEMORY_SELECT_CV_INPUT);

                auto recallButton = createLightParamCentered<VCVLightBezel<>>(Vec{}, module, RECALL_BUTTON_PARAM, RECALL_BUTTON_LIGHT);
                addSapphireParam(recallButton, "recall_button");

                auto storeButton = createLightParamCentered<VCVLightBezel<>>(Vec{}, module, STORE_BUTTON_PARAM, STORE_BUTTON_LIGHT);
                addSapphireParam(storeButton, "store_button");
           }
        };
    }
}

Model* modelSapphireChaops = createSapphireModel<Sapphire::ChaosOperators::ChaopsModule, Sapphire::ChaosOperators::ChaopsWidget>(
    "Chaops",
    Sapphire::ExpanderRole::ChaosOpSender
);
