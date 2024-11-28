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
            STORE_TRIGGER_INPUT,
            RECALL_TRIGGER_INPUT,
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
            GateTriggerReceiver storeReceiver;
            GateTriggerReceiver recallReceiver;

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
                configInput(STORE_TRIGGER_INPUT, "Store trigger");
                configInput(RECALL_TRIGGER_INPUT, "Recall trigger");
                initialize();
            }

            void initialize()
            {
                storeButtonPressed = false;
                recallButtonPressed = false;
                storeReceiver.initialize();
                recallReceiver.initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            int getMemoryIndex()
            {
                using namespace std;

                float cv = inputs[MEMORY_SELECT_CV_INPUT].getVoltageSum();
                float slider = params[MEMORY_SELECT_PARAM].getValue();
                float attenu = 2 * params[MEMORY_SELECT_ATTEN].getValue();
                if (isLowSensitive(MEMORY_SELECT_ATTEN))
                    attenu /= AttenuverterLowSensitivityDenom;
                slider += attenu * cv;
                int index = max(0u, static_cast<unsigned>(round(slider)) % MemoryCount);
                return index;
            }

            bool getStoreTrigger()
            {
                bool isButtonDown = (params[STORE_BUTTON_PARAM].getValue() > 0);
                bool buttonJustPressed = isButtonDown && !storeButtonPressed;
                storeButtonPressed = isButtonDown;
                bool triggerFired = storeReceiver.updateTrigger(inputs[STORE_TRIGGER_INPUT].getVoltageSum());
                return buttonJustPressed || triggerFired;
            }

            bool getRecallTrigger()
            {
                bool isButtonDown = (params[RECALL_BUTTON_PARAM].getValue() > 0);
                bool buttonJustPressed = isButtonDown && !recallButtonPressed;
                recallButtonPressed = isButtonDown;
                bool triggerFired = recallReceiver.updateTrigger(inputs[RECALL_TRIGGER_INPUT].getVoltageSum());
                return buttonJustPressed || triggerFired;
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

                    // HACK: "currentChannelCount" is really the displayed memory address.
                    currentChannelCount = message.memoryIndex;
                }
                else
                {
                    // No attached module, therefore no valid memory address to display.
                    currentChannelCount = -1;
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

                addSapphireInput(STORE_TRIGGER_INPUT, "store_trigger");
                addSapphireInput(RECALL_TRIGGER_INPUT, "recall_trigger");

                addSapphireChannelDisplay("memory_address_display");
           }
        };
    }
}

Model* modelSapphireChaops = createSapphireModel<Sapphire::ChaosOperators::ChaopsModule, Sapphire::ChaosOperators::ChaopsWidget>(
    "Chaops",
    Sapphire::ExpanderRole::ChaosOpSender
);
