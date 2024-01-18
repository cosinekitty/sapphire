#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "nucleus_engine.hpp"
#include "nucleus_init.hpp"

// Nucleus for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Nucleus
    {
        const std::size_t NUM_PARTICLES = 5;

        enum ParamId
        {
            SPEED_KNOB_PARAM,
            DECAY_KNOB_PARAM,
            MAGNET_KNOB_PARAM,
            IN_DRIVE_KNOB_PARAM,
            OUT_LEVEL_KNOB_PARAM,

            SPEED_ATTEN_PARAM,
            DECAY_ATTEN_PARAM,
            MAGNET_ATTEN_PARAM,
            IN_DRIVE_ATTEN_PARAM,
            OUT_LEVEL_ATTEN_PARAM,

            DC_REJECT_BUTTON_PARAM,

            AGC_LEVEL_PARAM,

            PARAMS_LEN
        };

        enum InputId
        {
            X_INPUT,
            Y_INPUT,
            Z_INPUT,

            SPEED_CV_INPUT,
            DECAY_CV_INPUT,
            MAGNET_CV_INPUT,
            IN_DRIVE_CV_INPUT,
            OUT_LEVEL_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            X1_OUTPUT,
            Y1_OUTPUT,
            Z1_OUTPUT,

            X2_OUTPUT,
            Y2_OUTPUT,
            Z2_OUTPUT,

            X3_OUTPUT,
            Y3_OUTPUT,
            Z3_OUTPUT,

            X4_OUTPUT,
            Y4_OUTPUT,
            Z4_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            DC_REJECT_BUTTON_LIGHT,

            LIGHTS_LEN
        };

        struct NucleusModule : Module
        {
            NucleusEngine engine{NUM_PARTICLES};
            AgcLevelQuantity *agcLevelQuantity{};
            bool enableLimiterWarning = true;

            NucleusModule()
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");

                configParam(SPEED_KNOB_PARAM, -6, +6, 0, "Speed");
                configParam(DECAY_KNOB_PARAM, 0, 1, 0.5, "Decay");
                configParam(MAGNET_KNOB_PARAM, -1, 1, 0, "Magnetic coupling");
                configParam(IN_DRIVE_KNOB_PARAM, 0, 2, 1, "Input drive", " dB", -10, 80);
                configParam(OUT_LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 80);

                configParam(SPEED_ATTEN_PARAM, -1, 1, 0, "Speed attenuverter", "%", 0, 100);
                configParam(DECAY_ATTEN_PARAM, -1, 1, 0, "Decay attenuverter", "%", 0, 100);
                configParam(MAGNET_ATTEN_PARAM, -1, 1, 0, "Magnetic coupling attenuverter", "%", 0, 100);
                configParam(IN_DRIVE_ATTEN_PARAM, -1, 1, 0, "Input drive attenuverter", "%", 0, 100);
                configParam(OUT_LEVEL_ATTEN_PARAM, -1, 1, 0, "Output level attenuverter", "%", 0, 100);

                configInput(SPEED_CV_INPUT, "Speed CV");
                configInput(DECAY_CV_INPUT, "Decay CV");
                configInput(MAGNET_CV_INPUT, "Magnetic coupling CV");
                configInput(IN_DRIVE_CV_INPUT, "Input level CV");
                configInput(OUT_LEVEL_CV_INPUT, "Output level CV");

                configOutput(X1_OUTPUT, "X1");
                configOutput(Y1_OUTPUT, "Y1");
                configOutput(Z1_OUTPUT, "Z1");
                configOutput(X2_OUTPUT, "X2");
                configOutput(Y2_OUTPUT, "Y2");
                configOutput(Z2_OUTPUT, "Z2");
                configOutput(X3_OUTPUT, "X3");
                configOutput(Y3_OUTPUT, "Y3");
                configOutput(Z3_OUTPUT, "Z3");
                configOutput(X4_OUTPUT, "X4");
                configOutput(Y4_OUTPUT, "Y4");
                configOutput(Z4_OUTPUT, "Z4");

                configButton(DC_REJECT_BUTTON_PARAM, "DC reject");

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

            json_t* dataToJson() override
            {
                json_t* root = json_object();
                json_object_set_new(root, "limiterWarningLight", json_boolean(enableLimiterWarning));
                agcLevelQuantity->save(root, "agcLevel");
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                // If the JSON is damaged, default to enabling the warning light.
                json_t *warningFlag = json_object_get(root, "limiterWarningLight");
                enableLimiterWarning = !json_is_false(warningFlag);
                agcLevelQuantity->load(root, "agcLevel");
            }

            void initialize()
            {
                params[DC_REJECT_BUTTON_PARAM].setValue(1.0f);

                engine.initialize();
                engine.enableFixedOversample(1);        // for smooth changes as SPEED knob is changed
                int rc = SetMinimumEnergy(engine);
                if (rc != 0)
                    WARN("SetMinimumEnergy returned error %d", rc);

                enableLimiterWarning = true;
                agcLevelQuantity->initialize();
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

            bool isEnabledDcReject() const
            {
                return params[DC_REJECT_BUTTON_PARAM].value > 0.5f;
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            float getControlValue(
                ParamId sliderId,
                ParamId attenuId,
                InputId cvInputId,
                float minSlider,
                float maxSlider)
            {
                float slider = params[sliderId].getValue();
                if (inputs[cvInputId].isConnected())
                {
                    float attenu = params[attenuId].getValue();
                    float cv = inputs[cvInputId].getVoltageSum();
                    // When the attenuverter is set to 100%, and the cv is +5V, we want
                    // to swing a slider that is all the way down (minSlider)
                    // to act like it is all the way up (maxSlider).
                    // Thus we allow the complete range of control for any CV whose
                    // range is [-5, +5] volts.
                    slider += attenu * (cv / 5.0) * (maxSlider - minSlider);
                }
                return slider;
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
                float knob = getControlValue(IN_DRIVE_KNOB_PARAM, IN_DRIVE_ATTEN_PARAM, IN_DRIVE_CV_INPUT, 0, 2);
                // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB) channels
                return std::pow(Clamp(knob, 0.0f, 2.0f), 4.0f);
            }

            float getOutputLevel()
            {
                float knob = getControlValue(OUT_LEVEL_KNOB_PARAM, OUT_LEVEL_ATTEN_PARAM, OUT_LEVEL_CV_INPUT, 0, 2);
                // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
                return std::pow(Clamp(knob, 0.0f, 2.0f), 4.0f);
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

            void process(const ProcessArgs& args) override
            {
                // Get current control settings.
                const float drive = getInputDrive();
                const float gain = getOutputLevel();
                const float halflife = getHalfLife();
                const float speed = getSpeedFactor();
                const float magnet = getMagneticCoupling();

                engine.setMagneticCoupling(magnet);

                // Feed the input (X, Y, Z) into the position of ball #1.
                // Scale the amplitude of the vector based on the input drive setting.
                Particle& input = engine.particle(0);
                input.pos[0] = drive * inputs[X_INPUT].getVoltageSum();
                input.pos[1] = drive * inputs[Y_INPUT].getVoltageSum();
                input.pos[2] = drive * inputs[Z_INPUT].getVoltageSum();
                input.pos[3] = 0;
                input.vel = PhysicsVector::zero();

                // Update the automatic gain control settings into the engine before updating state.
                // This order is required because engine.update() uses the AGC to moderate its outputs.
                reflectAgcSlider();

                // Allow the user to toggle the DC reject option at any time.
                // This will trigger an anti-click crossfade when needed.
                engine.setDcRejectEnabled(isEnabledDcReject());

                // Run the simulation for one time step.
                // Adjust the time step by the `speed` parameter,
                // so that the user can control the response over a wide range of frequencies.
                // Pass in the original sample rate however, because this is used
                // by the Automatic Gain Limiter to calculate decay constants for
                // the actual output stream (not simulated physical time).
                engine.update(speed * args.sampleTime, halflife, args.sampleRate, gain);

                // Let the DC reject pushbutton light reflect its button state.
                lights[DC_REJECT_BUTTON_LIGHT].setBrightness(isEnabledDcReject() ? 1.0f : 0.0f);

                // Report all output voltages to VCV Rack.

                outputs[X1_OUTPUT].setVoltage(engine.output(1, 0));
                outputs[Y1_OUTPUT].setVoltage(engine.output(1, 1));
                outputs[Z1_OUTPUT].setVoltage(engine.output(1, 2));

                outputs[X2_OUTPUT].setVoltage(engine.output(2, 0));
                outputs[Y2_OUTPUT].setVoltage(engine.output(2, 1));
                outputs[Z2_OUTPUT].setVoltage(engine.output(2, 2));

                outputs[X3_OUTPUT].setVoltage(engine.output(3, 0));
                outputs[Y3_OUTPUT].setVoltage(engine.output(3, 1));
                outputs[Z3_OUTPUT].setVoltage(engine.output(3, 2));

                outputs[X4_OUTPUT].setVoltage(engine.output(4, 0));
                outputs[Y4_OUTPUT].setVoltage(engine.output(4, 1));
                outputs[Z4_OUTPUT].setVoltage(engine.output(4, 2));
            }
        };


        class NucleusWarningLightWidget : public LightWidget
        {
        private:
            NucleusModule *nucleusModule;

            static int colorComponent(double scale, int lo, int hi)
            {
                return clamp(static_cast<int>(round(lo + scale*(hi-lo))), lo, hi);
            }

            NVGcolor warningColor(double distortion)
            {
                bool enableWarning = nucleusModule && nucleusModule->enableLimiterWarning;

                if (!enableWarning || distortion <= 0.0)
                    return nvgRGBA(0, 0, 0, 0);     // no warning light

                double decibels = 20.0 * std::log10(1.0 + distortion);
                double scale = clamp(decibels / 24.0);

                int red   = colorComponent(scale, 0x90, 0xff);
                int green = colorComponent(scale, 0x20, 0x50);
                int blue  = 0x00;
                int alpha = 0x70;

                return nvgRGBA(red, green, blue, alpha);
            }

        public:
            explicit NucleusWarningLightWidget(NucleusModule *module)
                : nucleusModule(module)
            {
                borderColor = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't draw a circular border
                bgColor     = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't mess with the knob behind the light
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                if (layer == 1)
                {
                    // Update the warning light state dynamically.
                    // Turn on the warning when the AGC is limiting the output.
                    double distortion = nucleusModule ? nucleusModule->engine.getAgcDistortion() : 0.0;
                    color = warningColor(distortion);
                }
                LightWidget::drawLayer(args, layer);
            }
        };


        struct NucleusWidget : SapphireReloadableModuleWidget
        {
            NucleusModule *nucleusModule;
            NucleusWarningLightWidget* warningLight{};

            explicit NucleusWidget(NucleusModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/nucleus.svg"))
                , nucleusModule(module)
            {
                setModule(module);

                addSapphireInput(X_INPUT, "x_input");
                addSapphireInput(Y_INPUT, "y_input");
                addSapphireInput(Z_INPUT, "z_input");

                addSapphireOutput(X1_OUTPUT, "x1_output");
                addSapphireOutput(Y1_OUTPUT, "y1_output");
                addSapphireOutput(Z1_OUTPUT, "z1_output");
                addSapphireOutput(X2_OUTPUT, "x2_output");
                addSapphireOutput(Y2_OUTPUT, "y2_output");
                addSapphireOutput(Z2_OUTPUT, "z2_output");
                addSapphireOutput(X3_OUTPUT, "x3_output");
                addSapphireOutput(Y3_OUTPUT, "y3_output");
                addSapphireOutput(Z3_OUTPUT, "z3_output");
                addSapphireOutput(X4_OUTPUT, "x4_output");
                addSapphireOutput(Y4_OUTPUT, "y4_output");
                addSapphireOutput(Z4_OUTPUT, "z4_output");

                addKnob(SPEED_KNOB_PARAM, "speed_knob");
                addKnob(DECAY_KNOB_PARAM, "decay_knob");
                addKnob(MAGNET_KNOB_PARAM, "magnet_knob");
                addKnob(IN_DRIVE_KNOB_PARAM, "in_drive_knob");

                // Superimpose a warning light on the output level knob.
                // We turn the warning light on when the limiter is distoring the output.
                auto levelKnob = addKnob(OUT_LEVEL_KNOB_PARAM, "out_level_knob");
                warningLight = new NucleusWarningLightWidget(module);
                warningLight->box.pos  = Vec(0.0f, 0.0f);
                warningLight->box.size = levelKnob->box.size;
                levelKnob->addChild(warningLight);

                addSapphireInput(SPEED_CV_INPUT, "speed_cv");
                addSapphireInput(DECAY_CV_INPUT, "decay_cv");
                addSapphireInput(MAGNET_CV_INPUT, "magnet_cv");
                addSapphireInput(IN_DRIVE_CV_INPUT, "in_drive_cv");
                addSapphireInput(OUT_LEVEL_CV_INPUT, "out_level_cv");

                addAttenuverter(SPEED_ATTEN_PARAM, "speed_atten");
                addAttenuverter(DECAY_ATTEN_PARAM, "decay_atten");
                addAttenuverter(MAGNET_ATTEN_PARAM, "magnet_atten");
                addAttenuverter(IN_DRIVE_ATTEN_PARAM, "in_drive_atten");
                addAttenuverter(OUT_LEVEL_ATTEN_PARAM, "out_level_atten");

                auto toggle = createLightParamCentered<VCVLightBezelLatch<>>(Vec{}, module, DC_REJECT_BUTTON_PARAM, DC_REJECT_BUTTON_LIGHT);
                addReloadableParam(toggle, "dc_reject_button");

                reloadPanel();
            }

            void appendContextMenu(Menu* menu) override
            {
                if (nucleusModule != nullptr)
                {
                    menu->addChild(new MenuSeparator);

                    if (nucleusModule->agcLevelQuantity)
                    {
                        // Add slider to adjust the AGC's level setting (5V .. 10V) or to disable AGC.
                        menu->addChild(new AgcLevelSlider(nucleusModule->agcLevelQuantity));

                        // Add an option to enable/disable the warning slider.
                        menu->addChild(createBoolPtrMenuItem<bool>("Limiter warning light", "", &nucleusModule->enableLimiterWarning));
                    }
                }
            }
        };
    }
}


Model* modelNucleus = createModel<Sapphire::Nucleus::NucleusModule, Sapphire::Nucleus::NucleusWidget>("Nucleus");
