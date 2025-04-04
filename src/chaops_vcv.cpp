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
            FREEZE_BUTTON_PARAM,
            MORPH_PARAM,
            MORPH_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            MEMORY_SELECT_CV_INPUT,
            STORE_TRIGGER_INPUT,
            RECALL_TRIGGER_INPUT,
            FREEZE_INPUT,
            MORPH_CV_INPUT,
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
            FREEZE_BUTTON_LIGHT,
            LIGHTS_LEN
        };

        struct ChaopsModule : SapphireModule
        {
            Sender sender;
            bool storeButtonPressed = false;
            bool recallButtonPressed = false;
            int storeFlashCounter = 0;
            int recallFlashCounter = 0;
            GateTriggerReceiver storeReceiver;
            GateTriggerReceiver recallReceiver;
            GateTriggerReceiver freezeReceiver;

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
                configParam(MORPH_PARAM, 0, 1, 0, "Morph position/velocity");
                configParam(MORPH_ATTEN, -1, +1, 0, "Morph attenuverter", "%", 0, 100);
                configInput(MORPH_CV_INPUT, "Morph CV");
                configToggleGroup(FREEZE_INPUT, FREEZE_BUTTON_PARAM, "Freeze", "Freeze gate");
                initialize();
            }

            void initialize()
            {
                storeButtonPressed = false;
                recallButtonPressed = false;
                storeFlashCounter = 0;
                recallFlashCounter = 0;
                storeReceiver.initialize();
                recallReceiver.initialize();
                freezeReceiver.initialize();
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

            float getMorph()
            {
                return getControlValue(MORPH_PARAM, MORPH_ATTEN, MORPH_CV_INPUT);
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

            static int flashDurationSamples(float sampleRate)
            {
                const float flashDurationSeconds = 0.05;
                return static_cast<int>(flashDurationSeconds * sampleRate);
            }

            void process(const ProcessArgs& args) override
            {
                bool frozen = false;

                if (sender.isReceiverConnectedOnRight())
                {
                    Message message;
                    message.memoryIndex = getMemoryIndex();
                    message.store = getStoreTrigger();
                    message.recall = getRecallTrigger();
                    message.freeze = frozen = updateToggleGroup(freezeReceiver, FREEZE_INPUT, FREEZE_BUTTON_PARAM);
                    message.morph = getMorph();
                    sender.send(message);

                    // HACK: "currentChannelCount" is really the displayed memory address.
                    currentChannelCount = message.memoryIndex;

                    if (message.store)
                        storeFlashCounter = flashDurationSamples(args.sampleRate);
                    else if (storeFlashCounter > 0)
                        --storeFlashCounter;

                    if (message.recall)
                        recallFlashCounter = flashDurationSamples(args.sampleRate);
                    else if (recallFlashCounter > 0)
                        --recallFlashCounter;
                }
                else
                {
                    // No attached module, therefore no valid memory address to display.
                    currentChannelCount = -1;
                    storeFlashCounter = 0;
                    recallFlashCounter = 0;
                }

                setLightBrightness(STORE_BUTTON_LIGHT, storeFlashCounter > 0);
                setLightBrightness(RECALL_BUTTON_LIGHT, recallFlashCounter > 0);
                setLightBrightness(FREEZE_BUTTON_LIGHT, frozen);
            }

            void setLightBrightness(LightId lightId, bool lit)
            {
                lights[lightId].setBrightness(lit ? 1.0f : 0.06f);
            }
        };


        struct ChaopsWidget : SapphireWidget
        {
            ChaopsModule* chaopsModule;
            const NVGcolor RECALL_BUTTON_COLOR = nvgRGB(0x40, 0xd0, 0x3e);

            explicit ChaopsWidget(ChaopsModule* module)
                : SapphireWidget("chaops", asset::plugin(pluginInstance, "res/chaops.svg"))
                , chaopsModule(module)
            {
                setModule(module);
                addSapphireFlatControlGroup("memsel", MEMORY_SELECT_PARAM, MEMORY_SELECT_ATTEN, MEMORY_SELECT_CV_INPUT);

                auto storeButton = createLightParamCentered<SapphireCaptionButton>(Vec{}, module, STORE_BUTTON_PARAM, STORE_BUTTON_LIGHT);
                storeButton->setCaption('S');
                storeButton->initBaseColor(SCHEME_RED);
                addSapphireParam(storeButton, "store_button");

                auto recallButton = createLightParamCentered<SapphireCaptionButton>(Vec{}, module, RECALL_BUTTON_PARAM, RECALL_BUTTON_LIGHT);
                recallButton->setCaption('R');
                recallButton->initBaseColor(RECALL_BUTTON_COLOR);
                addSapphireParam(recallButton, "recall_button");

                addSapphireInput(STORE_TRIGGER_INPUT, "store_trigger");
                addSapphireInput(RECALL_TRIGGER_INPUT, "recall_trigger");

                addToggleGroup("freeze", FREEZE_INPUT, FREEZE_BUTTON_PARAM, FREEZE_BUTTON_LIGHT, 'F', 7.5, SCHEME_BLUE);
                addSapphireFlatControlGroup("morph", MORPH_PARAM, MORPH_ATTEN, MORPH_CV_INPUT);
                addSapphireChannelDisplay("memory_address_display");
            }
        };
    }
}

Model* modelSapphireChaops = createSapphireModel<Sapphire::ChaosOperators::ChaopsModule, Sapphire::ChaosOperators::ChaopsWidget>(
    "Chaops",
    Sapphire::ExpanderRole::ChaosOpSender
);
