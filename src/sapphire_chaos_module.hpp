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


        template <typename circuit_t>
        struct ChaosModule : SapphireModule
        {
            circuit_t circuit;
            bool turboMode = false;
            ChaosOperators::Receiver receiver;
            ChaoticOscillatorState memory[ChaosOperators::MemoryCount];

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
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t *root = SapphireModule::dataToJson();
                json_object_set_new(root, "turboMode", json_boolean(turboMode));
                json_object_set_new(root, "chaosMode", json_integer(circuit.getMode()));

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
            }

            void process(const ProcessArgs& args) override
            {
                using namespace Sapphire::ChaosOperators;

                bool shouldUpdateCircuit = true;
                float morph = 0;        // 0 = position, 1 = velocity

                const Message* message = receiver.inboundMessage();
                if (message != nullptr)
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

                SlopeVector vel = circuit.velocity();

                float xmix = setFlippableOutputVoltage(X_OUTPUT, (1-morph)*circuit.xpos() + morph*vel.mx);
                float ymix = setFlippableOutputVoltage(Y_OUTPUT, (1-morph)*circuit.ypos() + morph*vel.my);
                float zmix = setFlippableOutputVoltage(Z_OUTPUT, (1-morph)*circuit.zpos() + morph*vel.mz);

                outputs.at(POLY_OUTPUT).setChannels(3);
                outputs.at(POLY_OUTPUT).setVoltage(xmix, 0);
                outputs.at(POLY_OUTPUT).setVoltage(ymix, 1);
                outputs.at(POLY_OUTPUT).setVoltage(zmix, 2);

                sendVector(xmix, ymix, zmix, shouldClearTricorder);

                shouldClearTricorder = false;
            }
        };


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

                menu->addChild(createIndexSubmenuItem(
                    "Chaos mode",
                    labels,
                    [=]() { return chaosModule->circuit.getMode(); },
                    [=](size_t mode) { chaosModule->circuit.setMode(mode); }
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
                if (chaosModule != nullptr)
                {
                    auto& c = chaosModule->circuit;
                    if (c.getModeCount() > 1)
                    {
                        const char *modeName = c.getModeName(c.getMode());
                        if (modeName != nullptr)
                            return modeName[0];
                    }
                }
                return '\0';
            }
        };


        template <typename module_t>
        inline ui::MenuItem* CreateTurboModeMenuItem(module_t* chaosModule)
        {
            return createBoolMenuItem(
                "Turbo mode: +5 speed",
                "",
                [=]() { return chaosModule->turboMode; },
                [=](bool state) { chaosModule->turboMode = state; }
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
                if (chaosModule != nullptr && chaosModule->turboMode)
                    return 'T';

                return '\0';
            }
        };


        struct SpeedAttenuverterKnob : SapphireAttenuverterKnob
        {
            Param* atten = nullptr;

            void appendContextMenu(ui::Menu* menu) override
            {
                SapphireAttenuverterKnob::appendContextMenu(menu);
                if (atten != nullptr)
                {
                    // The following lambda is called every time the menu item is clicked.
                    auto onMenuItemSelected = [this]()
                    {
                        // Disable low sensitivity if set, in order to get the correct percentage.
                        setLowSensitive(false);

                        // The attenuverter setting comes from CV of 5 volts swinging the speed by 14 octaves
                        // if the attenuverter were set to 100%. We want to bring the ratio down
                        // to 1 volt per octave by setting the attenuverter knob to the correct percentage.
                        const float cvRange = 5;
                        const float knobRange = 2*ChaosOctaveRange;
                        atten->setValue(cvRange/knobRange);
                    };
                    menu->addChild(createMenuItem("Snap to V/OCT", "", onMenuItemSelected));
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
                AddChaosOptionsToMenu(menu, chaosModule, false);
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
