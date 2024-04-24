#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "nucleus_shared.hpp"
#include "nucleus_engine.hpp"
#include "nucleus_init.hpp"
#include "nucleus_reset.hpp"
#include "polynucleus_panel.hpp"

// Polynucleus for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Polynucleus
    {
        enum ParamId
        {
            SPEED_KNOB_PARAM,
            DECAY_KNOB_PARAM,
            MAGNET_KNOB_PARAM,
            IN_DRIVE_KNOB_PARAM,
            OUT_LEVEL_KNOB_PARAM,
            VISC_KNOB_PARAM,
            SPIN_KNOB_PARAM,

            SPEED_ATTEN_PARAM,
            DECAY_ATTEN_PARAM,
            MAGNET_ATTEN_PARAM,
            IN_DRIVE_ATTEN_PARAM,
            OUT_LEVEL_ATTEN_PARAM,
            VISC_ATTEN_PARAM,
            SPIN_ATTEN_PARAM,

            AUDIO_MODE_BUTTON_PARAM,

            AGC_LEVEL_PARAM,
            DC_REJECT_PARAM,

            CLEAR_BUTTON_PARAM,

            PARAMS_LEN
        };

        enum InputId
        {
            A_INPUT,

            SPEED_CV_INPUT,
            DECAY_CV_INPUT,
            MAGNET_CV_INPUT,
            IN_DRIVE_CV_INPUT,
            OUT_LEVEL_CV_INPUT,
            VISC_CV_INPUT,
            SPIN_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            B_OUTPUT,
            C_OUTPUT,
            D_OUTPUT,
            E_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            AUDIO_MODE_BUTTON_LIGHT,
            CLEAR_BUTTON_LIGHT,

            LIGHTS_LEN
        };

        struct PolynucleusModule : SapphireAutomaticLimiterModule
        {
            NucleusEngine engine{Nucleus::NUM_PARTICLES};
            Nucleus::CrashChecker crashChecker;
            AgcLevelQuantity *agcLevelQuantity{};
            int tricorderOutputIndex = 1;     // 1..4: which output row to send to Tricorder
            bool resetTricorder{};
            DcRejectQuantity *dcRejectQuantity{};

            PolynucleusModule()
                : SapphireAutomaticLimiterModule(PARAMS_LEN)
            {
                using namespace Nucleus;

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(A_INPUT, "Particle A");

                configParam(SPEED_KNOB_PARAM, -6, +6, 0, "Speed");
                configParam(DECAY_KNOB_PARAM, 0, 1, 0.5, "Decay");
                configParam(MAGNET_KNOB_PARAM, -1, 1, 0, "Magnetic coupling");
                configParam(IN_DRIVE_KNOB_PARAM, 0, 2, 1, "Input drive", " dB", -10, 20*INPUT_EXPONENT);
                configParam(OUT_LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 20*OUTPUT_EXPONENT);
                configParam(VISC_KNOB_PARAM, 0, +1, 0, "Aether viscosity");
                configParam(SPIN_KNOB_PARAM, -1, 1, 0, "Aether rotation");

                configParam(SPEED_ATTEN_PARAM, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(DECAY_ATTEN_PARAM, -1, 1, 0, "Decay attenuverter", "%", 0, 100);
                configParam(MAGNET_ATTEN_PARAM, -1, 1, 0, "Magnetic coupling attenuverter", "%", 0, 100);
                configParam(IN_DRIVE_ATTEN_PARAM, -1, 1, 0, "Input drive attenuverter", "%", 0, 100);
                configParam(OUT_LEVEL_ATTEN_PARAM, -1, 1, 0, "Output level attenuverter", "%", 0, 100);

#if POLYNUCLEUS_ENABLE_EXTRA_CONTROLS
                configParam(VISC_ATTEN_PARAM, -1, 1, 0, "Aether viscosity attenuverter", "%", 0, 100);
                configParam(SPIN_ATTEN_PARAM, -1, 1, 0, "Aether rotation attenuverter", "%", 0, 100);
#endif

                dcRejectQuantity = configParam<DcRejectQuantity>(
                    DC_REJECT_PARAM,
                    DC_REJECT_MIN_FREQ,
                    DC_REJECT_MAX_FREQ,
                    DefaultCornerFrequencyHz,
                    "DC reject cutoff",
                    " Hz"
                );
                dcRejectQuantity->setValue(DefaultCornerFrequencyHz);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(DECAY_CV_INPUT, "Decay CV");
                configInput(MAGNET_CV_INPUT, "Magnetic coupling CV");
                configInput(IN_DRIVE_CV_INPUT, "Input level CV");
                configInput(OUT_LEVEL_CV_INPUT, "Output level CV");
                configInput(VISC_CV_INPUT, "Aether viscosity CV");
                configInput(SPIN_CV_INPUT, "Aether rotation CV");

                configOutput(B_OUTPUT, "Particle B");
                configOutput(C_OUTPUT, "Particle C");
                configOutput(D_OUTPUT, "Particle D");
                configOutput(E_OUTPUT, "Particle E");

                configButton(AUDIO_MODE_BUTTON_PARAM, "Toggle audio/CV output mode");
                configButton(CLEAR_BUTTON_PARAM, "Brings the simulation back to its quiet initial state");

                agcLevelQuantity = configParam<AgcLevelQuantity>(
                    AGC_LEVEL_PARAM,
                    AGC_LEVEL_MIN,
                    AGC_DISABLE_MAX,
                    AGC_LEVEL_DEFAULT,
                    "Output limiter"
                );
                agcLevelQuantity->value = AGC_LEVEL_DEFAULT;

                initialize();
            }

            double getAgcDistortion() const override
            {
                return engine.getAgcDistortion();
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireAutomaticLimiterModule::dataToJson();
                json_object_set_new(root, "limiterWarningLight", json_boolean(enableLimiterWarning));
                agcLevelQuantity->save(root, "agcLevel");
                dcRejectQuantity->save(root, "dcRejectFrequency");
                json_object_set_new(root, "tricorderOutputIndex", json_integer(tricorderOutputIndex));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                using namespace Nucleus;

                SapphireAutomaticLimiterModule::dataFromJson(root);

                // If the JSON is damaged, default to enabling the warning light.
                json_t *warningFlag = json_object_get(root, "limiterWarningLight");
                enableLimiterWarning = !json_is_false(warningFlag);

                agcLevelQuantity->load(root, "agcLevel");
                dcRejectQuantity->load(root, "dcRejectFrequency");

                resetTricorder = true;
                tricorderOutputIndex = 1;   // fallback
                json_t *tri = json_object_get(root, "tricorderOutputIndex");
                if (json_is_integer(tri))
                {
                    int index = json_integer_value(tri);
                    if (index > 0 && index < NUM_PARTICLES)
                        tricorderOutputIndex = index;
                }
            }

            void initialize()
            {
                using namespace Nucleus;

                params[AUDIO_MODE_BUTTON_PARAM].setValue(1.0f);
                params[CLEAR_BUTTON_PARAM].setValue(0.0f);

                engine.initialize();
                SetMinimumEnergy(engine);
                dcRejectQuantity->value = DefaultCornerFrequencyHz;
                dcRejectQuantity->changed = false;
                engine.setDcRejectCornerFrequency(DefaultCornerFrequencyHz);
                enableLimiterWarning = true;
                agcLevelQuantity->initialize();
                tricorderOutputIndex = 1;
                resetTricorder = true;
            }

            void resetSimulation()
            {
                engine.resetAfterCrash();
                Nucleus::SetMinimumEnergy(engine);
                resetTricorder = true;
            }

            void setOutputRow(int row)
            {
                using namespace Nucleus;

                if (row < 1 || row >= NUM_PARTICLES)
                    return;     // ignore invalid requests

                if (tricorderOutputIndex != row)
                {
                    tricorderOutputIndex = row;     // remember the selected row
                    resetTricorder = true;          // tell Tricorder to start over with a new graph
                }
            }

            void reflectAgcSlider()
            {
                // Check for changes to the automatic gain control: its level, and whether enabled/disabled.
                // Avoid unnecessary work by updating only when we know the user's desired level has changed.
                if (agcLevelQuantity && agcLevelQuantity->changed)
                {
                    bool enabled = agcLevelQuantity->isAgcEnabled();
                    if (enabled)
                        engine.setAgcLevel(agcLevelQuantity->clampedAgc());
                    engine.setAgcEnabled(enabled);
                    agcLevelQuantity->changed = false;
                }
            }

            bool isEnabledAudioMode() const
            {
                return params[AUDIO_MODE_BUTTON_PARAM].value > 0.5f;
            }

            bool isClearButtonPressed() const
            {
                return params[CLEAR_BUTTON_PARAM].value > 0.5f;
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            float getHalfLife()
            {
                float knob = getControlValue(DECAY_KNOB_PARAM, DECAY_ATTEN_PARAM, DECAY_CV_INPUT, 0, 1);
                const int minExp = -3;    // 0.001
                const int maxExp = +2;    // 100
                return std::pow(10.0f, minExp + (maxExp - minExp)*knob);
            }

            float getInputDrive()
            {
                using namespace Nucleus;

                float knob = getControlValue(IN_DRIVE_KNOB_PARAM, IN_DRIVE_ATTEN_PARAM, IN_DRIVE_CV_INPUT, 0, 2);
                // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
                return INPUT_SCALE * std::pow(knob, INPUT_EXPONENT);
            }

            float getOutputLevel()
            {
                using namespace Nucleus;

                float knob = getControlValue(OUT_LEVEL_KNOB_PARAM, OUT_LEVEL_ATTEN_PARAM, OUT_LEVEL_CV_INPUT, 0, 2);
                // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+36 dB)
                return std::pow(knob, OUTPUT_EXPONENT);
            }

            float getSpeedFactor()
            {
                float knob = getControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN_PARAM, SPEED_CV_INPUT, -6, +6);
                float factor = std::pow(2.0f, knob-1 /* -7..+5 is a better range */);
                return factor;
            }

            float getMagneticCoupling()
            {
                float knob = getControlValue(MAGNET_KNOB_PARAM, MAGNET_ATTEN_PARAM, MAGNET_CV_INPUT, -1, +1);
                const float scale = 0.09f;
                return knob * scale;
            }

            float getAetherVisc()
            {
                float knob = getControlValue(VISC_KNOB_PARAM, VISC_ATTEN_PARAM, VISC_CV_INPUT, 0, +1);
                return knob;
            }

            float getAetherSpin()
            {
                float knob = getControlValue(SPIN_KNOB_PARAM, SPIN_ATTEN_PARAM, SPIN_CV_INPUT, -1, +1);
                return knob;
            }

            void process(const ProcessArgs& args) override
            {
                lights[CLEAR_BUTTON_LIGHT].setBrightness(isClearButtonPressed() ? 1.0f : 0.0f);
                if (isClearButtonPressed())
                    resetSimulation();      // keep resetting as long as button is held down

                // Get current control settings.
                const float drive = getInputDrive();
                const float gain = getOutputLevel();
                const float halflife = getHalfLife();
                const float speed = getSpeedFactor();
                const float magnet = getMagneticCoupling();
                const float spin = getAetherSpin();
                const float visc = getAetherVisc();

                engine.setMagneticCoupling(magnet);
                engine.setAetherSpin(spin);
                engine.setAetherVisc(visc);

                if (dcRejectQuantity->changed)
                {
                    engine.setDcRejectCornerFrequency(dcRejectQuantity->value);
                    dcRejectQuantity->changed = false;
                }

                // Feed the input (X, Y, Z) into the position of ball #1.
                // Scale the amplitude of the vector based on the input drive setting.
                Particle& input = engine.particle(0);
                input.pos[0] = drive * inputs[A_INPUT].getVoltage(0);
                input.pos[1] = drive * inputs[A_INPUT].getVoltage(1);
                input.pos[2] = drive * inputs[A_INPUT].getVoltage(2);
                input.pos[3] = 0;
                input.vel = PhysicsVector::zero();

                // Update the automatic gain control settings into the engine before updating state.
                // This order is required because engine.update() uses the AGC to moderate its outputs.
                reflectAgcSlider();

                // Allow the user to toggle the audio/cv option at any time.
                // This will trigger an anti-click crossfade when needed.
                engine.setDcRejectEnabled(isEnabledAudioMode());

                // Run the simulation for one time step.
                // Adjust the time step by the `speed` parameter,
                // so that the user can control the response over a wide range of frequencies.
                // Pass in the original sample rate however, because this is used
                // by the Automatic Gain Limiter to calculate decay constants for
                // the actual output stream (not simulated physical time).
                engine.update(speed * args.sampleTime, halflife, args.sampleRate, gain);

                // Look for NAN/infinite outputs, and reset the engine as needed (every 10K samples, that is).
                if (crashChecker.check(engine))
                {
                    // We just "rebooted" the engine due to invalid output.
                    // Make the output knob glow bright pink for a little while.
                    recoveryCountdown = static_cast<int>(args.sampleRate);
                }
                else if (recoveryCountdown > 0)
                {
                    // We have reset the engine in the last second or so,
                    // so we keep the output knob bright pink until recoveryCountdown hits zero.
                    --recoveryCountdown;
                }

                // Let the audio/cv toggle pushbutton light reflect its button state.
                lights[AUDIO_MODE_BUTTON_LIGHT].setBrightness(isEnabledAudioMode() ? 1.0f : 0.0f);

                // Report all output voltages to VCV Rack.

                outputs[B_OUTPUT].setChannels(3);
                outputs[B_OUTPUT].setVoltage(engine.output(1, 0), 0);
                outputs[B_OUTPUT].setVoltage(engine.output(1, 1), 1);
                outputs[B_OUTPUT].setVoltage(engine.output(1, 2), 2);

                outputs[C_OUTPUT].setChannels(3);
                outputs[C_OUTPUT].setVoltage(engine.output(2, 0), 0);
                outputs[C_OUTPUT].setVoltage(engine.output(2, 1), 1);
                outputs[C_OUTPUT].setVoltage(engine.output(2, 2), 2);

                outputs[D_OUTPUT].setChannels(3);
                outputs[D_OUTPUT].setVoltage(engine.output(3, 0), 0);
                outputs[D_OUTPUT].setVoltage(engine.output(3, 1), 1);
                outputs[D_OUTPUT].setVoltage(engine.output(3, 2), 2);

                outputs[E_OUTPUT].setChannels(3);
                outputs[E_OUTPUT].setVoltage(engine.output(4, 0), 0);
                outputs[E_OUTPUT].setVoltage(engine.output(4, 1), 1);
                outputs[E_OUTPUT].setVoltage(engine.output(4, 2), 2);

                // Pass along the selected output to Tricorder, if attached to the right side...
                float tx = engine.output(tricorderOutputIndex, 0);
                float ty = engine.output(tricorderOutputIndex, 1);
                float tz = engine.output(tricorderOutputIndex, 2);
                bool reset = resetTricorder;
                resetTricorder = false;
                vectorSender.sendVector(tx, ty, tz, reset);
            }
        };


        struct PolynucleusWidget : SapphireReloadableModuleWidget
        {
            PolynucleusModule *polynucleusModule;
            WarningLightWidget* warningLight{};
            int hoverOutputIndex{};
            bool ownsMouse{};
            SvgOverlay* audioLabel;
            SvgOverlay* controlLabel;

            explicit PolynucleusWidget(PolynucleusModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/polynucleus.svg"))
                , polynucleusModule(module)
                , audioLabel(SvgOverlay::Load("res/polynucleus_label_audio.svg"))
                , controlLabel(SvgOverlay::Load("res/polynucleus_label_control.svg"))
            {
                setModule(module);
                addChild(audioLabel);
                addChild(controlLabel);

                // Leave "AUDIO" visible, but hide the overlapping "CONTROL".
                // Later, if the button is turned off, we will flip the
                // visibility states and show "CONTROL" instead.
                controlLabel->hide();

                addSapphireInput(A_INPUT, "a_input");

                addSapphireOutput(B_OUTPUT, "b_output");
                addSapphireOutput(C_OUTPUT, "c_output");
                addSapphireOutput(D_OUTPUT, "d_output");
                addSapphireOutput(E_OUTPUT, "e_output");

                addKnob(SPEED_KNOB_PARAM, "speed_knob");
                addKnob(DECAY_KNOB_PARAM, "decay_knob");
                addKnob(MAGNET_KNOB_PARAM, "magnet_knob");
                addKnob(IN_DRIVE_KNOB_PARAM, "in_drive_knob");
#if POLYNUCLEUS_ENABLE_EXTRA_CONTROLS
                addKnob(VISC_KNOB_PARAM, "visc_knob");
                addKnob(SPIN_KNOB_PARAM, "spin_knob");
#endif

                // Superimpose a warning light on the output level knob.
                // We turn the warning light on when the limiter is distoring the output.
                auto levelKnob = addKnob(OUT_LEVEL_KNOB_PARAM, "out_level_knob");
                warningLight = new WarningLightWidget(module);
                warningLight->box.pos  = Vec(0.0f, 0.0f);
                warningLight->box.size = levelKnob->box.size;
                levelKnob->addChild(warningLight);

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(DECAY_CV_INPUT, "decay_cv");
                addSapphireInput(MAGNET_CV_INPUT, "magnet_cv");
                addSapphireInput(IN_DRIVE_CV_INPUT, "in_drive_cv");
                addSapphireInput(OUT_LEVEL_CV_INPUT, "out_level_cv");

#if POLYNUCLEUS_ENABLE_EXTRA_CONTROLS
                addSapphireInput(VISC_CV_INPUT, "visc_cv");
                addSapphireInput(SPIN_CV_INPUT, "spin_cv");
#endif

                addSapphireAttenuverter(SPEED_ATTEN_PARAM, "speed_atten");
                addSapphireAttenuverter(DECAY_ATTEN_PARAM, "decay_atten");
                addSapphireAttenuverter(MAGNET_ATTEN_PARAM, "magnet_atten");
                addSapphireAttenuverter(IN_DRIVE_ATTEN_PARAM, "in_drive_atten");
                addSapphireAttenuverter(OUT_LEVEL_ATTEN_PARAM, "out_level_atten");

#if POLYNUCLEUS_ENABLE_EXTRA_CONTROLS
                addSapphireAttenuverter(VISC_ATTEN_PARAM, "visc_atten");
                addSapphireAttenuverter(SPIN_ATTEN_PARAM, "spin_atten");
#endif

                auto audioModeButton = createLightParamCentered<VCVLightBezelLatch<>>(Vec{}, module, AUDIO_MODE_BUTTON_PARAM, AUDIO_MODE_BUTTON_LIGHT);
                addReloadableParam(audioModeButton, "audio_mode_button");

                auto clearButton = createLightParamCentered<VCVLightBezel<>>(Vec{}, module, CLEAR_BUTTON_PARAM, CLEAR_BUTTON_LIGHT);
                addReloadableParam(clearButton, "clear_button");

                reloadPanel();
            }

            void appendContextMenu(Menu* menu) override
            {
                if (polynucleusModule != nullptr)
                {
                    menu->addChild(new MenuSeparator);

                    // Add slider to adjust the DC reject filter's corner frequency.
                    menu->addChild(new DcRejectSlider(polynucleusModule->dcRejectQuantity));

                    // Add slider to adjust the AGC's level setting (5V .. 10V) or to disable AGC.
                    menu->addChild(new AgcLevelSlider(polynucleusModule->agcLevelQuantity));

                    // Add an option to enable/disable the warning slider.
                    menu->addChild(createBoolPtrMenuItem<bool>("Limiter warning light", "", &polynucleusModule->enableLimiterWarning));

                    // Add an action to reset the simulation to its low energy state.
                    menu->addChild(createMenuItem(
                        "Reset simulation",
                        "",
                        [=]{ polynucleusModule->resetSimulation(); }
                    ));
                }
            }

            bool isVectorReceiverConnectedOnRight() const
            {
                return (polynucleusModule != nullptr) && polynucleusModule->isVectorReceiverConnectedOnRight();
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                // Allow everything underneath to draw first.
                SapphireReloadableModuleWidget::drawLayer(args, layer);

                if (layer == 1 && isVectorReceiverConnectedOnRight())
                {
                    // Draw luminous output selectors on top of the base layers.

                    if (ownsMouse)
                        drawOutputRowCursor(args.vg, hoverOutputIndex);

                    drawOutputRowSelectionBox(args.vg, polynucleusModule->tricorderOutputIndex);
                }
            }

            Rect outputRowBoundingBox(int row) const
            {
                using namespace Panel;

                Rect r;
                r.pos.x = mm2px(X1Out - DxLeft);
                r.pos.y = mm2px(Y1Out - DyOut/2 + (row-1)*DyOut);
                r.size.x = mm2px(DxTotal);
                r.size.y = mm2px(DyOut);

                return r;
            }

            Rect mouseTargetBoundingBox(int row) const
            {
                Rect box = outputRowBoundingBox(row);
                const float frac = 0.29;
                const float dx = box.size.x;
                box.size.x *= frac;
                box.pos.x += dx * (1 - frac);
                return box;
            }

            void drawOutputRowSelectionBox(NVGcontext *vg, int row)
            {
                using namespace Nucleus;

                if (row < 1 || row >= NUM_PARTICLES)
                    return;     // ignore invalid requests

                Rect box = outputRowBoundingBox(row);

                // Draw a rightward-facing arrow that fits inside the bounding box.
                const float A = 0.80 * box.size.x;       // fraction rightward where the arrowhead begins
                const float H = 0.38 * box.size.y;       // fraction of vertical distance for skinny side of arrow
                const float G = 0.10 * box.size.y;       // fraction of vertical margin above and below the arrow corners
                const float V = 0.71 * box.size.x;       // fraction of horizontal distance to the left of the arrow
                const float W = 0.02 * box.size.x;       // fraction of horizontal distance to the right of the arrow

                const float x1 = box.pos.x + V;
                const float x2 = box.pos.x + A;
                const float x3 = box.pos.x + box.size.x - W;

                const float y1 = box.pos.y + G;
                const float yc = box.pos.y + box.size.y/2;
                const float y2 = yc - H/2;
                const float y3 = yc + H/2;
                const float y4 = box.pos.y + box.size.y - G;

                nvgBeginPath(vg);
                nvgStrokeColor(vg, SCHEME_BLACK);
                nvgFillColor(vg, nvgRGB(0xc0, 0xa0, 0x20));
                nvgMoveTo(vg, x1, y2);
                nvgLineTo(vg, x2, y2);
                nvgLineTo(vg, x2, y1);
                nvgLineTo(vg, x3, yc);
                nvgLineTo(vg, x2, y4);
                nvgLineTo(vg, x2, y3);
                nvgLineTo(vg, x1, y3);
                nvgClosePath(vg);
                nvgStroke(vg);
                nvgFill(vg);
            }

            void drawOutputRowCursor(NVGcontext *vg, int row)
            {
                using namespace Sapphire::Nucleus;
                using namespace Sapphire::Polynucleus::Panel;

                if (row < 1 || row >= NUM_PARTICLES)
                    return;     // ignore invalid requests

                Rect box = outputRowBoundingBox(row);

                const float radius = box.size.x / 20;

                nvgBeginPath(vg);
                nvgStrokeColor(vg, SCHEME_YELLOW);
                nvgFillColor(vg, SCHEME_YELLOW);
                nvgStrokeWidth(vg, 1.0f);
                nvgLineCap(vg, NVG_ROUND);
                nvgMoveTo(vg, box.pos.x + radius, box.pos.y);
                nvgLineTo(vg, box.pos.x + (box.size.x - radius), box.pos.y);
                nvgArcTo(vg, box.pos.x + box.size.x, box.pos.y, box.pos.x + box.size.x, box.pos.y + (box.size.y - radius), radius);
                nvgLineTo(vg, box.pos.x + box.size.x, box.pos.y + (box.size.y - radius));
                nvgArcTo(vg, box.pos.x + box.size.x, box.pos.y + box.size.y, box.pos.x + (box.size.x - radius), box.pos.y + box.size.y, radius);
                nvgLineTo(vg, box.pos.x + (box.size.x - radius), box.pos.y + box.size.y);
                nvgArcTo(vg, box.pos.x, box.pos.y + box.size.y, box.pos.x, box.pos.y + (box.size.y - radius), radius);
                nvgLineTo(vg, box.pos.x, box.pos.y + radius);
                nvgArcTo(vg, box.pos.x, box.pos.y, box.pos.x + radius, box.pos.y, radius);
                nvgClosePath(vg);
                nvgStroke(vg);
            }

            void onHover(const HoverEvent& e) override
            {
                using namespace Nucleus;

                SapphireReloadableModuleWidget::onHover(e);

                // Check to see if the mouse cursor is within any of the bounding rectangles.
                hoverOutputIndex = 0;   // indicate none match
                for (int row = 1; row < NUM_PARTICLES; ++row)
                {
                    Rect box = mouseTargetBoundingBox(row);
                    if (box.contains(e.pos))
                    {
                        hoverOutputIndex = row;     // found a matching row!
                        break;
                    }
                }
            }

            void onEnter(const EnterEvent& e) override
            {
                ownsMouse = true;
                SapphireReloadableModuleWidget::onEnter(e);
            }

            void onLeave(const LeaveEvent& e) override
            {
                ownsMouse = false;
                SapphireReloadableModuleWidget::onLeave(e);
            }

            void onButton(const ButtonEvent& e) override
            {
                using namespace Nucleus;

                SapphireReloadableModuleWidget::onButton(e);

                if ((polynucleusModule != nullptr) && isVectorReceiverConnectedOnRight())
                {
                    // See if the mouse click lands inside any of the mouse bounding boxes.
                    for (int row = 1; row < NUM_PARTICLES; ++row)
                    {
                        Rect box = mouseTargetBoundingBox(row);
                        if (box.contains(e.pos))
                        {
                            // Select the new output row.
                            polynucleusModule->setOutputRow(row);
                            break;
                        }
                    }
                }
            }

            void step() override
            {
                if (polynucleusModule != nullptr)
                {
                    // Toggle between showing "AUDIO" or "CONTROL" depending on the mode button.
                    bool audio = polynucleusModule->isEnabledAudioMode();
                    if (audio != audioLabel->isVisible())
                    {
                        // Toggle which of the two labels is showing.
                        audioLabel->setVisible(audio);
                        controlLabel->setVisible(!audio);
                    }
                }
                SapphireReloadableModuleWidget::step();
            }
        };
    }
}


Model* modelPolynucleus = createSapphireModel<Sapphire::Polynucleus::PolynucleusModule, Sapphire::Polynucleus::PolynucleusWidget>(
    "Polynucleus",
    Sapphire::VectorRole::Sender
);
