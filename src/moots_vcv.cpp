#include "sapphire_vcvrack.hpp"
#include "sapphire_engine.hpp"
#include "sapphire_widget.hpp"

// Sapphire Moots for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Moots
    {
        enum ParamId
        {
            TOGGLEBUTTON1_PARAM,
            TOGGLEBUTTON2_PARAM,
            TOGGLEBUTTON3_PARAM,
            TOGGLEBUTTON4_PARAM,
            TOGGLEBUTTON5_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            INAUDIO1_INPUT,
            INAUDIO2_INPUT,
            INAUDIO3_INPUT,
            INAUDIO4_INPUT,
            INAUDIO5_INPUT,
            CONTROL1_INPUT,
            CONTROL2_INPUT,
            CONTROL3_INPUT,
            CONTROL4_INPUT,
            CONTROL5_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            OUTAUDIO1_OUTPUT,
            OUTAUDIO2_OUTPUT,
            OUTAUDIO3_OUTPUT,
            OUTAUDIO4_OUTPUT,
            OUTAUDIO5_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            MOOTLIGHT1,
            MOOTLIGHT2,
            MOOTLIGHT3,
            MOOTLIGHT4,
            MOOTLIGHT5,
            LIGHTS_LEN
        };

        enum class ControlMode
        {
            Gate,
            Trigger,
        };

        struct MootsModule : SapphireModule
        {
            static const int NUM_CONTROLLERS = 5;
            static_assert(NUM_CONTROLLERS == PARAMS_LEN,   "Incorrect number of entries in `enum ParamId`");
            static_assert(2*NUM_CONTROLLERS == INPUTS_LEN, "Incorrect number of entries in `enum InputId`");
            static_assert(NUM_CONTROLLERS == OUTPUTS_LEN,  "Incorrect number of entries in `enum OutputId`");
            static_assert(NUM_CONTROLLERS == LIGHTS_LEN,   "Incorrect number of entries in `enum LightId`");

            ControlMode controlMode = ControlMode::Gate;
            bool isActive[NUM_CONTROLLERS]{};
            GateTriggerReceiver controlReceiver[NUM_CONTROLLERS];
            Slewer slewer[NUM_CONTROLLERS];

            MootsModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                initialize();
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configButton(TOGGLEBUTTON1_PARAM, "Moot 1");
                configButton(TOGGLEBUTTON2_PARAM, "Moot 2");
                configButton(TOGGLEBUTTON3_PARAM, "Moot 3");
                configButton(TOGGLEBUTTON4_PARAM, "Moot 4");
                configButton(TOGGLEBUTTON5_PARAM, "Moot 5");
                configInput(INAUDIO1_INPUT, "Signal 1");
                configInput(INAUDIO2_INPUT, "Signal 2");
                configInput(INAUDIO3_INPUT, "Signal 3");
                configInput(INAUDIO4_INPUT, "Signal 4");
                configInput(INAUDIO5_INPUT, "Signal 5");
                configInput(CONTROL1_INPUT, "Control 1");
                configInput(CONTROL2_INPUT, "Control 2");
                configInput(CONTROL3_INPUT, "Control 3");
                configInput(CONTROL4_INPUT, "Control 4");
                configInput(CONTROL5_INPUT, "Control 5");
                configOutput(OUTAUDIO1_OUTPUT, "Signal 1");
                configOutput(OUTAUDIO2_OUTPUT, "Signal 2");
                configOutput(OUTAUDIO3_OUTPUT, "Signal 3");
                configOutput(OUTAUDIO4_OUTPUT, "Signal 4");
                configOutput(OUTAUDIO5_OUTPUT, "Signal 5");

                for (int i = 0; i < NUM_CONTROLLERS; ++i)
                    configBypass(INAUDIO1_INPUT + i, OUTAUDIO1_OUTPUT + i);
            }

            void initialize()
            {
                controlMode = ControlMode::Gate;
                for (int i = 0; i < NUM_CONTROLLERS; ++i)
                {
                    isActive[i] = false;
                    controlReceiver[i].initialize();
                    slewer[i].reset();
                }
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            bool getRampingOption(int buttonIndex) const
            {
                return
                    (buttonIndex >= 0) &&
                    (buttonIndex < NUM_CONTROLLERS) &&
                    slewer[buttonIndex].isEnabled();
            }

            void setRampingOption(int buttonIndex, bool enable)
            {
                if (buttonIndex < 0 || buttonIndex >= NUM_CONTROLLERS)
                    return;

                Slewer& s = slewer[buttonIndex];
                if (enable)
                    s.enable(isActive[buttonIndex]);
                else
                    s.reset();
            }

            void onSampleRateChange(const SampleRateChangeEvent& e) override
            {
                // We slew using a linear ramp over a time span of 1/400 of a second.
                // Round to the nearest integer number of samples for the current sample rate.
                int newRampLength = static_cast<int>(round(e.sampleRate / 400.0f));

                for (int i = 0; i < NUM_CONTROLLERS; ++i)
                    slewer[i].setRampLength(newRampLength);
            }

            void process(const ProcessArgs& args) override
            {
                float volts[PORT_MAX_CHANNELS];

                for (int i = 0; i < NUM_CONTROLLERS; ++i)
                {
                    auto & control = inputs[CONTROL1_INPUT + i];

                    if (control.isConnected())
                    {
                        const float gv = control.getVoltageSum();
                        controlReceiver[i].update(gv);
                        if (controlMode == ControlMode::Gate)
                        {
                            isActive[i] = controlReceiver[i].isGateActive();
                        }
                        else    // trigger mode
                        {
                            if (controlReceiver[i].isTriggerActive())
                                isActive[i] = !isActive[i];
                        }
                    }
                    else
                    {
                        // When no control input is connected, allow the manual pushbutton take control.
                        isActive[i] = (params[TOGGLEBUTTON1_PARAM + i].getValue() > 0.0f);
                    }

                    // When a controller is turned on, make the push-button light bright,
                    // whether it's active because of the push-button or a control voltage.
                    // When a controller is turned off, use a very dim light, but not
                    // complete darkness. Some users like turning room brightness
                    // down very low, yet they still want to see where all 5 buttons are.
                    lights[MOOTLIGHT1 + i].setBrightness(isActive[i] ? 1.0f : 0.03f);

                    auto & outp = outputs[OUTAUDIO1_OUTPUT + i];

                    if (slewer[i].update(isActive[i]))
                    {
                        auto & inp = inputs[INAUDIO1_INPUT + i];
                        inp.readVoltages(volts);
                        outp.channels = inp.getChannels();
                        slewer[i].process(volts, outp.channels);
                        outp.writeVoltages(volts);
                    }
                    else
                    {
                        outp.channels = 0;	// force zero-channel output : shows "ghost" cable(s)
                    }
                }
            }

            static const char *FormatControl(ControlMode mode)
            {
                switch (mode)
                {
                case ControlMode::Trigger:
                    return "trigger";

                case ControlMode::Gate:
                default:
                    return "gate";
                }
            }

            static ControlMode ParseControl(const char *text)
            {
                if (text != nullptr)
                {
                    if (!strcmp(text, "trigger"))
                        return ControlMode::Trigger;
                }
                return ControlMode::Gate;
            }

            json_t* dataToJson() override
            {
                // Persist the audio slewing checkbox states.
                json_t* root = SapphireModule::dataToJson();
                json_t* flagList = json_array();

                for (int i = 0; i < NUM_CONTROLLERS; ++i)
                    json_array_append_new(flagList, json_boolean(slewer[i].isEnabled()));

                json_object_set_new(root, "slew", flagList);
                json_object_set_new(root, "controlMode", json_string(FormatControl(controlMode)));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);

                // Load the checkbox states for the audio slewers.
                json_t* flagList = json_object_get(root, "slew");
                if (json_is_array(flagList) && json_array_size(flagList) == NUM_CONTROLLERS)
                {
                    for (int i = 0; i < NUM_CONTROLLERS; ++i)
                    {
                        json_t *flag = json_array_get(flagList, i);
                        if (json_is_boolean(flag))
                        {
                            bool enable = json_boolean_value(flag);
                            if (enable)
                                slewer[i].enable(false);
                            else
                                slewer[i].reset();
                        }
                    }
                }

                json_t* cmode = json_object_get(root, "controlMode");
                const char *text = json_string_value(cmode);
                controlMode = ParseControl(text);
            }

            void onBypass(const BypassEvent&) override
            {
                // When the user bypasses Moots, we need to adjust each
                // output jack to have the same number of channels as the
                // corresponding input jack.
                // Otherwise, if we have turned off the given controller,
                // VCV Rack sees the output channel count is zero and thinks
                // there is no cable connected. That would prevent the input
                // from bypassing to the output correctly.

                for (int i = 0; i < NUM_CONTROLLERS; ++i)
                    outputs[OUTAUDIO1_OUTPUT + i].channels = inputs[INAUDIO1_INPUT + i].getChannels();
            }
        };

        using base_button_t = VCVLightBezelLatch<>;

        struct MootsButtonWidget : base_button_t
        {
            MootsModule* module = nullptr;
            int buttonIndex = -1;

            void appendContextMenu(Menu* menu) override
            {
                if (module == nullptr)
                    return;

                if (buttonIndex < 0 || buttonIndex >= MootsModule::NUM_CONTROLLERS)
                    return;

                menu->addChild(createBoolMenuItem(
                    "Anti-click ramping",
                    "",
                    [=]()
                    {
                        return module->getRampingOption(buttonIndex);
                    },
                    [=](bool state)
                    {
                        module->setRampingOption(buttonIndex, state);
                    }
                ));
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                base_button_t::drawLayer(args, layer);
                if (layer == 1)
                {
                    if (module && module->getRampingOption(buttonIndex))
                    {
                        // Draw a "ramp" symbol on top of the corresponding button.
                        const float f = 0.38;
                        const float e = 0.32;
                        const float ax = f * box.size.x;
                        const float ay = (1-e) * box.size.y;
                        const float bx = (1-f) * box.size.x;
                        const float by = e * box.size.y;
                        const float h = 0.15 * box.size.x;

                        nvgBeginPath(args.vg);
                        nvgStrokeColor(args.vg, SCHEME_BLACK);
                        nvgStrokeWidth(args.vg, 1.75f);
                        nvgMoveTo(args.vg, ax-h, ay);
                        nvgLineTo(args.vg, ax, ay);
                        nvgLineTo(args.vg, bx, by);
                        nvgLineTo(args.vg, bx+h, by);
                        nvgStroke(args.vg);
                    }
                }
            }
        };


        struct MootsWidget : SapphireWidget
        {
            MootsModule* mootsModule = nullptr;
            SvgOverlay* gateLabel = nullptr;
            SvgOverlay* triggerLabel = nullptr;

            explicit MootsWidget(MootsModule* module)
                : SapphireWidget("moots", asset::plugin(pluginInstance, "res/moots.svg"))
                , mootsModule(module)
            {
                setModule(module);

                gateLabel = SvgOverlay::Load("res/moots_label_gate.svg");
                addChild(gateLabel);

                triggerLabel = SvgOverlay::Load("res/moots_label_trigger.svg");
                triggerLabel->hide();
                addChild(triggerLabel);

                addMootsButton(25.05,  17.25, TOGGLEBUTTON1_PARAM, MOOTLIGHT1, 0);
                addMootsButton(25.05,  38.75, TOGGLEBUTTON2_PARAM, MOOTLIGHT2, 1);
                addMootsButton(25.05,  60.25, TOGGLEBUTTON3_PARAM, MOOTLIGHT3, 2);
                addMootsButton(25.05,  81.75, TOGGLEBUTTON4_PARAM, MOOTLIGHT4, 3);
                addMootsButton(25.05, 103.25, TOGGLEBUTTON5_PARAM, MOOTLIGHT5, 4);

                addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  17.25)), module, INAUDIO1_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  38.75)), module, INAUDIO2_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  60.25)), module, INAUDIO3_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  81.75)), module, INAUDIO4_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50, 103.25)), module, INAUDIO5_INPUT));

                addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  25.25)), module, CONTROL1_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  46.75)), module, CONTROL2_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  68.25)), module, CONTROL3_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  89.75)), module, CONTROL4_INPUT));
                addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05, 111.25)), module, CONTROL5_INPUT));

                addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  17.25)), module, OUTAUDIO1_OUTPUT));
                addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  38.75)), module, OUTAUDIO2_OUTPUT));
                addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  60.25)), module, OUTAUDIO3_OUTPUT));
                addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  81.75)), module, OUTAUDIO4_OUTPUT));
                addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60, 103.25)), module, OUTAUDIO5_OUTPUT));
            }

            void addMootsButton(float cx, float cy, ParamId paramId, LightId lightId, int buttonIndex)
            {
                MootsButtonWidget* button = createLightParamCentered<MootsButtonWidget>(
                    mm2px(Vec(cx, cy)),
                    mootsModule,
                    paramId,
                    lightId
                );
                button->module = mootsModule;
                button->buttonIndex = buttonIndex;
                addParam(button);
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);

                if (mootsModule == nullptr)
                    return;

                //---------------------------------------------------------------------------
                // Gate/Trigger control mode toggle

                menu->addChild(createBoolMenuItem(
                    "Use triggers for control",
                    "",
                    [=]()
                    {
                        return mootsModule->controlMode == ControlMode::Trigger;
                    },
                    [=](bool state)
                    {
                        mootsModule->controlMode = state ? ControlMode::Trigger : ControlMode::Gate;
                    }
                ));

                //---------------------------------------------------------------------------
                // Add the 5 menu items for anti-clicking ramping.

                menu->addChild(new MenuSeparator);

                for (int i = 0; i < MootsModule::NUM_CONTROLLERS; ++i)
                {
                    menu->addChild(createBoolMenuItem(
                        "Anti-click ramping on #" + std::to_string(i+1),
                        "",
                        [=]()
                        {
                            return mootsModule->getRampingOption(i);
                        },
                        [=](bool state)
                        {
                            mootsModule->setRampingOption(i, state);
                        }
                    ));
                }
            }

            void step() override
            {
                if (mootsModule && gateLabel && triggerLabel)
                {
                    // Toggle between showing "GATE" or "TRIGGER" depending on the toggle state.
                    bool showGate = (mootsModule->controlMode == ControlMode::Gate);
                    if (gateLabel->isVisible() != showGate)
                    {
                        gateLabel->setVisible(showGate);
                        triggerLabel->setVisible(!showGate);
                    }
                }
                ModuleWidget::step();
            }
        };
    }
}

Model* modelSapphireMoots = createSapphireModel<Sapphire::Moots::MootsModule, Sapphire::Moots::MootsWidget>(
    "Moots",
    Sapphire::ExpanderRole::None
);
