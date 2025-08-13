#pragma once
#include "sapphire_engine.hpp"
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_simd.hpp"
#include "chaos.hpp"

// Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire
// Generic pattern for a Sapphire-style vector-output chaos module.

namespace Sapphire
{
    namespace Chaos
    {
        const int ChaosOctaveRange = 7;     // the number of octaves above *OR* below zero.

        enum ParamId
        {
            // Large knobs for manual parameter adjustment
            SPEED_KNOB_PARAM,
            CHAOS_KNOB_PARAM,

            // Attenuverter knobs
            SPEED_ATTEN,
            CHAOS_ATTEN,

            // Parameters used by Zoo only.
            DILATE_PARAM,
            X_TRANSLATE_PARAM,
            Y_TRANSLATE_PARAM,
            Z_TRANSLATE_PARAM,

            PARAMS_LEN
        };

        enum InputId
        {
            // Control group CV inputs
            SPEED_CV_INPUT,
            CHAOS_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            X_OUTPUT,
            Y_OUTPUT,
            Z_OUTPUT,
            POLY_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        inline bool IsSafeCoord(double s)
        {
            return std::isfinite(s) && (std::abs(s) <= 100.0);
        }


        struct DilateSlider : SapphireSlider
        {
            explicit DilateSlider(SapphireQuantity* _quantity)
                : SapphireSlider(_quantity, "adjust output vector magnitude")
                {}
        };


        struct TranslateSlider : SapphireSlider
        {
            explicit TranslateSlider(SapphireQuantity* _quantity, const char* _varname)
                : SapphireSlider(_quantity, std::string("shift output vector ") + _varname)
                {}
        };


        template <typename circuit_t>
        struct ChaosModule : SapphireModule
        {
            static constexpr float maxVoltage = 100;
            static constexpr float maxVoltageSquared = maxVoltage * maxVoltage;

            circuit_t circuit;
            bool turboMode = false;
            ChaosOperators::Receiver receiver;
            ChaoticOscillatorState memory[ChaosOperators::MemoryCount];
            SapphireQuantity* dilateQuantity{};
            SapphireQuantity* xTranslateQuantity{};
            SapphireQuantity* yTranslateQuantity{};
            SapphireQuantity* zTranslateQuantity{};
            bool initialLocationFromMemory = false;     // Zoo needs this to allow custom configuration of starting point
            bool overflowed = false;
            bool flashPanelOnOverflow = true;

            ChaosModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                , receiver(*this)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");
                configOutput(POLY_OUTPUT, "Polyphonic (X, Y, Z)");

                configParam(SPEED_KNOB_PARAM, -ChaosOctaveRange, +ChaosOctaveRange, 0, "Speed");
                configParam(CHAOS_KNOB_PARAM, -1, +1, 0, "Chaos");

                configParam(SPEED_ATTEN, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(CHAOS_ATTEN, -1, 1, 0, "Chaos attenuverter", "%", 0, 100);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(CHAOS_CV_INPUT, "Chaos CV");

                initialize();
            }

            void initialize()
            {
                circuit.initialize();
                for (unsigned i = 0; i < ChaosOperators::MemoryCount; ++i)
                    memory[i] = circuit.getState();
                turboMode = false;
                shouldClearTricorder = true;
                flashPanelOnOverflow = true;
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void addDilateQuantity()
            {
                assert(dilateQuantity == nullptr);
                dilateQuantity = configParam<SapphireQuantity>(
                    Chaos::ParamId::DILATE_PARAM,
                    0.02, 5.0, 1.0,
                    "Output vector magnitude"
                );
                dilateQuantity->value = circuit.getDilate();
                dilateQuantity->changed = true;
            }

            void addTranslateQuantities()
            {
                assert(xTranslateQuantity == nullptr);
                assert(yTranslateQuantity == nullptr);
                assert(zTranslateQuantity == nullptr);

                SlopeVector t = circuit.getTranslate();

                xTranslateQuantity = configParam<SapphireQuantity>(
                    Chaos::ParamId::X_TRANSLATE_PARAM,
                    -30, +30, 0,
                    "Output vector translate x"
                );
                xTranslateQuantity->value = t.mx;
                xTranslateQuantity->changed = true;

                yTranslateQuantity = configParam<SapphireQuantity>(
                    Chaos::ParamId::Y_TRANSLATE_PARAM,
                    -30, +30, 0,
                    "Output vector translate y"
                );
                yTranslateQuantity->value = t.my;
                yTranslateQuantity->changed = true;

                zTranslateQuantity = configParam<SapphireQuantity>(
                    Chaos::ParamId::Z_TRANSLATE_PARAM,
                    -30, +30, 0,
                    "Output vector translate z"
                );
                zTranslateQuantity->value = t.mz;
                zTranslateQuantity->changed = true;
            }

            json_t* dataToJson() override
            {
                json_t *root = SapphireModule::dataToJson();
                json_object_set_new(root, "turboMode", json_boolean(turboMode));
                json_object_set_new(root, "chaosMode", json_integer(circuit.getMode()));
                jsonSetBool(root, "flashPanelOnOverflow", flashPanelOnOverflow);

                // Save the memory cells as a JSON array.
                json_t* memoryArray = json_array();
                for (unsigned i = 0; i < ChaosOperators::MemoryCount; ++i)
                {
                    json_t* cell = json_object();
                    json_object_set_new(cell, "x", json_real(memory[i].x));
                    json_object_set_new(cell, "y", json_real(memory[i].y));
                    json_object_set_new(cell, "z", json_real(memory[i].z));
                    json_array_append_new(memoryArray, cell);
                }
                json_object_set_new(root, "memory", memoryArray);

                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                jsonLoadBool(root, "flashPanelOnOverflow", flashPanelOnOverflow);

                json_t* flag = json_object_get(root, "turboMode");
                turboMode = json_is_true(flag);

                json_t* mode = json_object_get(root, "chaosMode");
                circuit.setMode(
                    json_is_integer(mode) ?
                    static_cast<int>(json_integer_value(mode)) :
                    circuit.getLegacyMode()
                );

                json_t* memoryArray = json_object_get(root, "memory");
                if (json_is_array(memoryArray))
                {
                    const unsigned n = static_cast<unsigned>(json_array_size(memoryArray));
                    const unsigned limit = std::min(n, ChaosOperators::MemoryCount);
                    for (unsigned i = 0; i < limit; ++i)
                    {
                        json_t* cell = json_array_get(memoryArray, i);
                        json_t* jx = json_object_get(cell, "x");
                        json_t* jy = json_object_get(cell, "y");
                        json_t* jz = json_object_get(cell, "z");
                        if (json_is_real(jx) && json_is_real(jy) && json_is_real(jz))
                        {
                            double xpos = json_real_value(jx);
                            double ypos = json_real_value(jy);
                            double zpos = json_real_value(jz);
                            memory[i] = ChaoticOscillatorState(xpos, ypos, zpos);
                        }
                    }
                }

                if (initialLocationFromMemory)      // Sapphire Zoo needs a user-configurable starting state
                    circuit.setState(memory[0]);

                shouldClearTricorder = true;    // so Tricorder gets cleared after loading a preset
            }

            void process(const ProcessArgs& args) override
            {
                using namespace Sapphire::ChaosOperators;

                bool shouldUpdateCircuit = true;
                float morph = 0;        // 0 = position, 1 = velocity

                const Message* message = receiver.inboundMessage();
                if (message)
                {
                    if (message->freeze)
                        shouldUpdateCircuit = false;

                    if (message->store)
                    {
                        ChaoticOscillatorState& mc = memory[message->memoryIndex % MemoryCount];
                        mc = circuit.getState();
                    }

                    if (message->recall)
                    {
                        const ChaoticOscillatorState& mc = memory[message->memoryIndex % MemoryCount];
                        circuit.setState(mc);
                        shouldUpdateCircuit = false;
                        shouldClearTricorder = true;
                    }

                    morph = message->morph;
                }

                if (shouldUpdateCircuit)
                {
                    float chaos = getControlValue(CHAOS_KNOB_PARAM, CHAOS_ATTEN, CHAOS_CV_INPUT, -1, +1);
                    circuit.setKnob(chaos);
                    float speed = getControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN, SPEED_CV_INPUT, -ChaosOctaveRange, +ChaosOctaveRange);
                    if (turboMode)
                        speed += 5;
                    double dt = args.sampleTime * TwoToPower(speed);
                    circuit.update(dt);
                }

                if (dilateQuantity && dilateQuantity->isChangedOneShot())
                    circuit.setDilate(dilateQuantity->value);

                if (xTranslateQuantity && yTranslateQuantity && zTranslateQuantity)
                {
                    // Intentionally bypass short-circuit || operation.
                    // I want all one-shots to happen and reset at the same time.
                    // Otherwise we could up with 3 consecutive updates when only 1 is needed.
                    // In C++, bool converts to int: false==0, true==1.
                    int xShot = xTranslateQuantity->isChangedOneShot();
                    int yShot = yTranslateQuantity->isChangedOneShot();
                    int zShot = zTranslateQuantity->isChangedOneShot();
                    if (xShot + yShot + zShot)
                    {
                        circuit.setTranslate(SlopeVector(
                            xTranslateQuantity->value,
                            yTranslateQuantity->value,
                            zTranslateQuantity->value
                        ));
                    }
                }

                SlopeVector vel = circuit.velocity();
                float xmix = (1-morph)*circuit.xpos() + morph*vel.mx;
                float ymix = (1-morph)*circuit.ypos() + morph*vel.my;
                float zmix = (1-morph)*circuit.zpos() + morph*vel.mz;

                float radiusSquared = xmix*xmix + ymix*ymix + zmix*zmix;
                if (!std::isfinite(radiusSquared) || radiusSquared > maxVoltageSquared)
                {
                    // Auto-reset.
                    circuit.initialize();
                    vel = circuit.velocity();
                    xmix = (1-morph)*circuit.xpos() + morph*vel.mx;
                    ymix = (1-morph)*circuit.ypos() + morph*vel.my;
                    zmix = (1-morph)*circuit.zpos() + morph*vel.mz;

                    // Communicate to the widget that a reset has happend.
                    // It will flash the panel for us.
                    overflowed = true;
                }

                xmix = setFlippableOutputVoltage(X_OUTPUT, xmix);
                ymix = setFlippableOutputVoltage(Y_OUTPUT, ymix);
                zmix = setFlippableOutputVoltage(Z_OUTPUT, zmix);

                outputs.at(POLY_OUTPUT).setChannels(3);
                outputs.at(POLY_OUTPUT).setVoltage(xmix, 0);
                outputs.at(POLY_OUTPUT).setVoltage(ymix, 1);
                outputs.at(POLY_OUTPUT).setVoltage(zmix, 2);

                sendVector(xmix, ymix, zmix, shouldClearTricorder);

                shouldClearTricorder = false;
            }
        };


        template <typename module_t>
        struct ChaosModeAction : history::Action
        {
            const int64_t moduleId;
            const int oldMode;
            const int newMode;

            explicit ChaosModeAction(const module_t* _chaosModule, int _newMode)
                : moduleId(_chaosModule->id)
                , oldMode(_chaosModule->circuit.getMode())
                , newMode(_newMode)
            {
                name = "change chaos mode";
            }

            void setChaosMode(int mode)
            {
                if (module_t* chaosModule = FindSapphireModule<module_t>(moduleId))
                    chaosModule->circuit.setMode(mode);
            }

            void undo() override
            {
                setChaosMode(oldMode);
            }

            void redo() override
            {
                setChaosMode(newMode);
            }
        };

        template <typename module_t>
        MenuItem* createChaosModeChooser(
            module_t* module,
            std::string text,
            std::vector<std::string> labels,
            std::function<size_t()> getter,
            std::function<void(size_t val)> setter,
            bool disabled = false,
            bool alwaysConsume = false)
        {
            struct Item : MenuItem
            {
                std::function<size_t()> getter;
                std::function<void(size_t)> setter;
                std::vector<std::string> labels;
                bool alwaysConsume;
                module_t* module;

                void step() override
                {
                    size_t currIndex = getter();
                    std::string label = (currIndex < labels.size()) ? labels[currIndex] : "";
                    this->rightText = label + "  " + RIGHT_ARROW;
                    MenuItem::step();
                }

                ui::Menu *createChildMenu() override
                {
                    ui::Menu *menu = new ui::Menu;
                    for (size_t i = 0; i < labels.size(); i++)
                    {
                        menu->addChild(createCheckMenuItem(
                            labels[i],
                            "",
                            [=](){ return getter() == i; },
                            [=](){ setter(i); },
                            !module->circuit.isModeEnabled(i),
                            alwaysConsume)
                        );
                    }
                    return menu;
                }
            };

            Item *item = createMenuItem<Item>(text);
            item->getter = getter;
            item->setter = setter;
            item->labels = labels;
            item->disabled = disabled;
            item->alwaysConsume = alwaysConsume;
            item->module = module;
            return item;
        }

        template <typename module_t>
        inline void AddChaosOptionsToMenu(Menu *menu, module_t *chaosModule, bool separator)
        {
            if (menu == nullptr)
                return;

            if (chaosModule == nullptr)
                return;

            // If this chaos module supports multiple parameter modes,
            // enumerate them now as menu options.
            const int numModes = chaosModule->circuit.getModeCount();
            if (numModes > 1)
            {
                if (separator)
                    menu->addChild(new MenuSeparator);

                std::vector<std::string> labels;
                for (int mode = 0; mode < numModes; ++mode)
                    labels.push_back(chaosModule->circuit.getModeName(mode));

                menu->addChild(createChaosModeChooser<module_t>(
                    chaosModule,
                    "Chaos mode",
                    labels,
                    [=]() { return chaosModule->circuit.getMode(); },
                    [=](size_t state)
                    {
                        int chaosMode = static_cast<int>(state);
                        if (chaosModule->circuit.getMode() != chaosMode)
                            InvokeAction(new ChaosModeAction<module_t>(chaosModule, chaosMode));
                    }
                ));
            }
        }


        template <typename module_t>
        struct ChaosKnob : SapphireCaptionKnob
        {
            module_t* chaosModule = nullptr;

            void appendContextMenu(Menu* menu) override
            {
                AddChaosOptionsToMenu(menu, chaosModule, true);
            }

            char getCaption() const override
            {
                if (chaosModule)
                {
                    auto& c = chaosModule->circuit;
                    if (c.getModeCount() > 1)
                    {
                        const char *modeName = c.getModeName(c.getMode());
                        if (modeName)
                            return modeName[0];
                    }
                }
                return '\0';
            }
        };


        template <typename module_t>
        inline MenuItem* CreateTurboModeMenuItem(module_t* chaosModule)
        {
            return createBoolMenuItem(
                "Turbo mode: +5 speed",
                "",
                [=]()
                {
                    return chaosModule->turboMode;
                },
                [=](bool state)
                {
                    if (chaosModule->turboMode != state)
                        InvokeAction(new BoolToggleAction(chaosModule->turboMode, "turbo mode"));
                }
            );
        }


        template <typename module_t>
        inline MenuItem* CreateFlashToggleMenuItem(module_t* chaosModule)
        {
            return createBoolMenuItem(
                "Flash panel when overflow reset occurs",
                "",
                [=]()
                {
                    return chaosModule->flashPanelOnOverflow;
                },
                [=](bool state)
                {
                    if (chaosModule->flashPanelOnOverflow != state)
                        InvokeAction(new BoolToggleAction(chaosModule->flashPanelOnOverflow, "toggle panel flash on overflow"));
                }
            );
        }


        template <typename module_t>
        struct SpeedKnob : SapphireCaptionKnob
        {
            module_t* chaosModule = nullptr;

            void appendContextMenu(Menu* menu) override
            {
                menu->addChild(new MenuSeparator);
                menu->addChild(CreateTurboModeMenuItem(chaosModule));
            }

            char getCaption() const override
            {
                if (chaosModule && chaosModule->turboMode)
                    return 'T';

                return '\0';
            }
        };


        struct SnapVoctAction : history::Action
        {
            Param* atten{};
            SapphireAttenuverterKnob* knob{};
            const bool prevLowSensitive;
            const float prevAttenValue;

            explicit SnapVoctAction(Param* _atten, SapphireAttenuverterKnob* _knob)
                : atten(_atten)
                , knob(_knob)
                , prevLowSensitive(_knob && _knob->isLowSensitive())
                , prevAttenValue(_atten ? _atten->getValue() : 0.0f)
            {
                name = "snap attenuverter to V/OCT";
            }

            void undo() override
            {
                if (atten && knob)
                {
                    atten->setValue(prevAttenValue);
                    knob->setLowSensitive(prevLowSensitive);
                }
            }

            void redo() override
            {
                if (atten && knob)
                {
                    // Disable low sensitivity if set, in order to get the correct percentage.
                    knob->setLowSensitive(false);

                    // The attenuverter setting comes from CV of 5 volts swinging the speed by 14 octaves
                    // if the attenuverter were set to 100%. We want to bring the ratio down
                    // to 1 volt per octave by setting the attenuverter knob to the correct percentage.
                    const float cvRange = 5;
                    const float knobRange = 2*ChaosOctaveRange;
                    atten->setValue(cvRange/knobRange);
                }
            }
        };


        struct SpeedAttenuverterKnob : SapphireAttenuverterKnob
        {
            Param* atten = nullptr;

            void appendContextMenu(Menu* menu) override
            {
                SapphireAttenuverterKnob::appendContextMenu(menu);
                if (atten)
                {
                    menu->addChild(createMenuItem(
                        "Snap to V/OCT",
                        "",
                        [=]()
                        {
                            InvokeAction(new SnapVoctAction(atten, this));
                        }
                    ));
                }
            }
        };


        template <typename module_t>
        struct ChaosWidget : SapphireWidget
        {
            module_t* chaosModule{};

            ChaosWidget(module_t* module, const char *moduleCode, const char *panelSvgFileName)
                : SapphireWidget(moduleCode, asset::plugin(pluginInstance, panelSvgFileName))
                , chaosModule(module)
            {
                setModule(module);

                addFlippableOutputPort(X_OUTPUT, "x_output", module);
                addFlippableOutputPort(Y_OUTPUT, "y_output", module);
                addFlippableOutputPort(Z_OUTPUT, "z_output", module);

                addSapphireOutput(POLY_OUTPUT, "p_output");

                // SPEED knob: provide Turbo Mode context menu checkbox.
                using speed_knob_t = SpeedKnob<module_t>;
                speed_knob_t* speedKnob = addKnob<speed_knob_t>(SPEED_KNOB_PARAM, "speed_knob");
                speedKnob->chaosModule = module;

                // CHAOS knob: include Chaos Mode selection in the context menu.
                using chaos_knob_t = ChaosKnob<module_t>;
                chaos_knob_t* chaosKnob = addKnob<chaos_knob_t>(CHAOS_KNOB_PARAM, "chaos_knob");
                chaosKnob->chaosModule = module;

                auto knob = addSapphireAttenuverter<SpeedAttenuverterKnob>(SPEED_ATTEN, "speed_atten");
                knob->atten = module ? &module->params.at(SPEED_ATTEN) : nullptr;

                addSapphireAttenuverter(CHAOS_ATTEN, "chaos_atten");

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(CHAOS_CV_INPUT, "chaos_cv");
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
                if (chaosModule == nullptr)
                    return;

                // The SPEED knob has Turbo Mode in its context menu.
                // The CHAOS knob has mode selector in its context menu.
                // We put both of these in the panel menu also, because
                // people often explore the right-click menu of the
                // panel, but don't think to do that for knobs.

                menu->addChild(CreateTurboModeMenuItem<module_t>(chaosModule));
                menu->addChild(CreateFlashToggleMenuItem<module_t>(chaosModule));
                AddChaosOptionsToMenu(menu, chaosModule, false);
            }

            void step() override
            {
                SapphireWidget::step();
                if (chaosModule && chaosModule->overflowed)
                {
                    chaosModule->overflowed = false;
                    if (chaosModule->flashPanelOnOverflow)
                        splash.begin(0xb0, 0x10, 0x00);
                }
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);
                float xcol = 1.7;
                drawFlipIndicator(args, X_OUTPUT, xcol,  88.0);
                drawFlipIndicator(args, Y_OUTPUT, xcol,  97.0);
                drawFlipIndicator(args, Z_OUTPUT, xcol, 106.0);
            }

            void drawFlipIndicator(const DrawArgs& args, int outputId, float x, float y)
            {
                if (chaosModule && chaosModule->getVoltageFlipEnabled(outputId))
                {
                    const float dx = 0.75;
                    nvgBeginPath(args.vg);
                    nvgStrokeColor(args.vg, SCHEME_BLACK);
                    nvgStrokeWidth(args.vg, 1.0);
                    nvgMoveTo(args.vg, mm2px(x-dx), mm2px(y));
                    nvgLineTo(args.vg, mm2px(x+dx), mm2px(y));
                    nvgStroke(args.vg);
                }
            }
        };
    }
}
