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
            FREEZE_PORTMODE_BUTTON_PARAM,
            CRUISE_PARAM,
            CRUISE_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            MEMORY_SELECT_CV_INPUT,
            STORE_TRIGGER_INPUT,
            RECALL_TRIGGER_INPUT,
            FREEZE_INPUT,
            MORPH_CV_INPUT,
            CRUISE_CV_INPUT,
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
            ToggleGroup freezeToggleGroup;

            ChaopsModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                , sender(*this)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configParam(MEMORY_SELECT_PARAM, 0, MemoryCount-1, 0, "Memory select");
                paramQuantities.at(MEMORY_SELECT_PARAM)->snapEnabled = true;
                configAtten(MEMORY_SELECT_ATTEN, "Memory select");
                configInput(MEMORY_SELECT_CV_INPUT, "Memory select CV");
                configButton(STORE_BUTTON_PARAM, "Store");
                configButton(RECALL_BUTTON_PARAM, "Recall");
                configButton(FREEZE_PORTMODE_BUTTON_PARAM);
                configInput(STORE_TRIGGER_INPUT, "Store trigger");
                configInput(RECALL_TRIGGER_INPUT, "Recall trigger");
                configParam(MORPH_PARAM, 0, 1, 0, "Morph position/velocity");
                configAtten(MORPH_ATTEN, "Morph");
                configInput(MORPH_CV_INPUT, "Morph CV");
                configParam(CRUISE_PARAM, 0, 1, 0, "Cruise");
                configAtten(CRUISE_ATTEN, "Cruise");
                configInput(CRUISE_CV_INPUT, "Cruise CV");
                freezeToggleGroup.config(this, "Freeze", "freezeToggleGroup", FREEZE_INPUT, FREEZE_BUTTON_PARAM, FREEZE_BUTTON_LIGHT, "Freeze", "");
                freezeToggleGroup.addPortMenuItems = false;
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
                freezeToggleGroup.initialize();
                params.at(FREEZE_BUTTON_PARAM).setValue(0);
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                freezeToggleGroup.jsonSave(root);
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                freezeToggleGroup.jsonLoad(root);

                // A little bit of a hack. Echo also uses ToggleGroup with a gate/trigger input mode toggle.
                // I don't want to break Echo, but I want to replace the custom-json serialization
                // with an ordinary button parameter in Chaops.
                // Strategy: save/load json the same way, but at runtime, we use the button.
                // That means I need to sync the button state with the custom json now.
                params.at(FREEZE_PORTMODE_BUTTON_PARAM).setValue(static_cast<float>(freezeToggleGroup.mode));
            }

            int getMemoryIndex()
            {
                float cv = inputs.at(MEMORY_SELECT_CV_INPUT).getVoltageSum();
                float slider = params.at(MEMORY_SELECT_PARAM).getValue();
                float attenu = 2 * params.at(MEMORY_SELECT_ATTEN).getValue();
                if (isLowSensitive(MEMORY_SELECT_ATTEN))
                    attenu /= AttenuverterLowSensitivityDenom;
                slider += attenu * cv;
                int index = std::max(0u, static_cast<unsigned>(std::round(slider)) % MemoryCount);
                return index;
            }

            float getMorph()
            {
                return getControlValue(MORPH_PARAM, MORPH_ATTEN, MORPH_CV_INPUT);
            }

            float getCruise()
            {
                return getControlValue(CRUISE_PARAM, CRUISE_ATTEN, CRUISE_CV_INPUT);
            }

            bool getStoreTrigger()
            {
                bool isButtonDown = (params.at(STORE_BUTTON_PARAM).getValue() > 0);
                bool buttonJustPressed = isButtonDown && !storeButtonPressed;
                storeButtonPressed = isButtonDown;
                storeReceiver.update(inputs.at(STORE_TRIGGER_INPUT).getVoltageSum());
                return buttonJustPressed || storeReceiver.isTriggerActive();
            }

            bool getRecallTrigger()
            {
                bool isButtonDown = (params.at(RECALL_BUTTON_PARAM).getValue() > 0);
                bool buttonJustPressed = isButtonDown && !recallButtonPressed;
                recallButtonPressed = isButtonDown;
                recallReceiver.update(inputs.at(RECALL_TRIGGER_INPUT).getVoltageSum());
                return buttonJustPressed || recallReceiver.isTriggerActive();
            }

            static int flashDurationSamples(float sampleRate)
            {
                return static_cast<int>(FlashDurationSeconds * sampleRate);
            }

            bool updateFreezeToggleGroup()
            {
                // Keep the gate/trigger option up to date from the toggle button.
                // This is a hack for backward compatibility:
                // Before the tiny toggle button existed, this was a menu option only
                // and did not use a VCV parameter.
                // To keep the file format the same, we let freezeToggleGroup's json
                // be the single-source-of-truth, and keep copying the button state
                // into freezeToggleGroup.mode.

                freezeToggleGroup.mode = isFreezeInputTriggerMode()
                    ? ToggleGroupMode::Trigger
                    : ToggleGroupMode::Gate;

                return freezeToggleGroup.process();
            }

            void process(const ProcessArgs& args) override
            {
                bool frozen = updateFreezeToggleGroup();

                if (sender.isReceiverConnectedOnRight())
                {
                    Message message;
                    message.memoryIndex = getMemoryIndex();
                    message.store = getStoreTrigger();
                    message.recall = getRecallTrigger();
                    message.freeze = frozen;
                    message.morph = getMorph();
                    message.cruise = getCruise();
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

            bool isFreezeInputTriggerMode() const
            {
                return params.at(FREEZE_PORTMODE_BUTTON_PARAM).value > 0.5f;
            }

            void updateTooltips()
            {
                updateToggleButtonTooltip(FREEZE_PORTMODE_BUTTON_PARAM, "CV input mode: GATE", "CV input mode: TRIGGER");
                updateInputTooltip(FREEZE_INPUT, isFreezeInputTriggerMode() ? "Freeze trigger" : "Freeze gate");
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

                ToggleGroup* freezeGroup = module ? &(module->freezeToggleGroup) : nullptr;
                addToggleGroup(
                    freezeGroup,
                    "freeze",
                    FREEZE_INPUT,
                    FREEZE_BUTTON_PARAM,
                    FREEZE_BUTTON_LIGHT,
                    FREEZE_PORTMODE_BUTTON_PARAM,
                    'F',
                    7.5,
                    SCHEME_BLUE
                );
                addSapphireFlatControlGroup("morph", MORPH_PARAM, MORPH_ATTEN, MORPH_CV_INPUT);
                addSapphireFlatControlGroup("cruise", CRUISE_PARAM, CRUISE_ATTEN, CRUISE_CV_INPUT);
                addSapphireChannelDisplay("memory_address_display");
            }

            void step() override
            {
                SapphireWidget::step();
                if (chaopsModule)
                    chaopsModule->updateTooltips();
            }
        };
    }
}

Model* modelSapphireChaops = createSapphireModel<Sapphire::ChaosOperators::ChaopsModule, Sapphire::ChaosOperators::ChaopsWidget>(
    "Chaops",
    Sapphire::ExpanderRole::ChaosOpSender
);
