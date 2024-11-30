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
            PARAMS_LEN
        };

        enum InputId
        {
            MEMORY_SELECT_CV_INPUT,
            STORE_TRIGGER_INPUT,
            RECALL_TRIGGER_INPUT,
            FREEZE_INPUT,
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
                configButton(FREEZE_BUTTON_PARAM, "Freeze");
                configInput(STORE_TRIGGER_INPUT, "Store trigger");
                configInput(RECALL_TRIGGER_INPUT, "Recall trigger");
                configInput(FREEZE_INPUT, "Freeze gate");
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

            bool getFreezeGate()
            {
                bool portActive = freezeReceiver.updateGate(inputs[FREEZE_INPUT].getVoltageSum());
                bool buttonActive = (params[FREEZE_BUTTON_PARAM].getValue() > 0);
                // Allow the button to toggle the gate state, so the gate can be active-low or active-high.
                return portActive ^ buttonActive;
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
                    message.freeze = frozen = getFreezeGate();
                    sender.send(message);

                    // HACK: "currentChannelCount" is really the displayed memory address.
                    currentChannelCount = message.memoryIndex;

                    const float flashDurationSeconds = 0.05;

                    if (message.store)
                        storeFlashCounter = static_cast<int>(args.sampleRate * flashDurationSeconds);
                    else if (storeFlashCounter > 0)
                        --storeFlashCounter;

                    if (message.recall)
                        recallFlashCounter = static_cast<int>(args.sampleRate * flashDurationSeconds);
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

                lights[STORE_BUTTON_LIGHT ].setBrightness(storeFlashCounter  ? 1.0f : 0.03f);
                lights[RECALL_BUTTON_LIGHT].setBrightness(recallFlashCounter ? 1.0f : 0.03f);
                lights[FREEZE_BUTTON_LIGHT].setBrightness(frozen ? 1.0f : 0.03f);
            }
        };


        using letter_button_base_t = VCVLightBezel<>;
        struct LetterButton : letter_button_base_t
        {
            std::string fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
            char caption[2]{};
            float dx{};

            void drawLayer(const DrawArgs& args, int layer) override
            {
                letter_button_base_t::drawLayer(args, layer);

                if (caption[0])
                {
                    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
                    if (font)
                    {
                        nvgFontSize(args.vg, 15);
                        nvgFontFaceId(args.vg, font->handle);
                        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 0xff));
                        nvgText(args.vg, dx, 15.0, caption, caption+1);
                    }
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

                auto storeButton = createLightParamCentered<LetterButton>(Vec{}, module, STORE_BUTTON_PARAM, STORE_BUTTON_LIGHT);
                storeButton->caption[0] = 'S';
                storeButton->dx = 7.2;
                addSapphireParam(storeButton, "store_button");

                auto recallButton = createLightParamCentered<LetterButton>(Vec{}, module, RECALL_BUTTON_PARAM, RECALL_BUTTON_LIGHT);
                recallButton->caption[0] = 'R';
                recallButton->dx = 7.4;
                addSapphireParam(recallButton, "recall_button");

                auto freezeButton = createLightParamCentered<LetterButton>(Vec{}, module, FREEZE_BUTTON_PARAM, FREEZE_BUTTON_LIGHT);
                freezeButton->momentary = false;
                freezeButton->latch = true;
                freezeButton->caption[0] = 'F';
                freezeButton->dx = 7.2;
                addSapphireParam(freezeButton, "freeze_button");

                addSapphireInput(STORE_TRIGGER_INPUT, "store_trigger");
                addSapphireInput(RECALL_TRIGGER_INPUT, "recall_trigger");
                addSapphireInput(FREEZE_INPUT, "freeze_input");

                addSapphireChannelDisplay("memory_address_display");
            }
        };
    }
}

Model* modelSapphireChaops = createSapphireModel<Sapphire::ChaosOperators::ChaopsModule, Sapphire::ChaosOperators::ChaopsWidget>(
    "Chaops",
    Sapphire::ExpanderRole::ChaosOpSender
);
