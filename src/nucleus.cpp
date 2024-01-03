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

        const float DC_REJECT_CUTOFF_HZ = 10.0f;

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

        using NucleusDcRejectFilter = StagedFilter<float, 3>;

        struct NucleusRow
        {
            NucleusDcRejectFilter filter[3];    // vindex: [0]=X, [1]=Y, [2]=Z.

            NucleusRow()
            {
                initialize();
            }

            void initialize()
            {
                for (int i = 0; i < 3; ++i)
                {
                    filter[i].SetCutoffFrequency(DC_REJECT_CUTOFF_HZ);
                    filter[i].Reset();
                }
            }
        };

        struct NucleusModule : Module
        {
            NucleusEngine engine{NUM_PARTICLES};
            NucleusRow row[NUM_PARTICLES]{};

            NucleusModule()
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");

                configParam(SPEED_KNOB_PARAM, -7, +7, 0, "Speed");
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

                initialize();
            }

            void initialize()
            {
                params[DC_REJECT_BUTTON_PARAM].setValue(1.0f);

                for (std::size_t i = 0; i < NUM_PARTICLES; ++i)
                    row[i].initialize();

                int rc = SetMinimumEnergy(engine);
                if (rc != 0)
                    WARN("SetMinimumEnergy returned error %d", rc);
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
                // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
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
                float knob = getControlValue(SPEED_KNOB_PARAM, SPEED_ATTEN_PARAM, SPEED_CV_INPUT, -7, +7);
                float factor = std::pow(2.0f, knob);
                return factor;
            }

            float getMagneticCoupling()
            {
                float knob = getControlValue(MAGNET_KNOB_PARAM, MAGNET_ATTEN_PARAM, MAGNET_CV_INPUT, -1, +1);
                const float scale = 0.015f;
                return knob * scale;
            }

            void copyOutput(float sampleRate, OutputId outputId, float gain, int pindex, int vindex)
            {
                const Particle& p = engine.particle(pindex);
                outputs[outputId].setChannels(1);
                float vOut = gain * p.pos[vindex];

                if (isEnabledDcReject())
                {
                    // Run through high-pass filter to remove DC content.
                    vOut = row[pindex].filter[vindex].UpdateHiPass(vOut, sampleRate);
                }

                outputs[outputId].setVoltage(vOut, 0);
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

                // Run the simulation for one time step.
                // Adjust the time step by the `speed` parameter,
                // so that the user can control the response over a wide range of frequencies.
                // Compensate for time dilation by multiplying speed and halflife:
                // when running at 10x speed, we want the halflife to correspond
                // to real time, not sim time.
                engine.update(speed * args.sampleTime, speed * halflife);

                // Let the pushbutton light reflect the button state.
                lights[DC_REJECT_BUTTON_LIGHT].setBrightness(isEnabledDcReject() ? 1.0f : 0.0f);

                // Copy all the outputs.

                copyOutput(args.sampleRate, X1_OUTPUT, gain, 1, 0);
                copyOutput(args.sampleRate, Y1_OUTPUT, gain, 1, 1);
                copyOutput(args.sampleRate, Z1_OUTPUT, gain, 1, 2);

                copyOutput(args.sampleRate, X2_OUTPUT, gain, 2, 0);
                copyOutput(args.sampleRate, Y2_OUTPUT, gain, 2, 1);
                copyOutput(args.sampleRate, Z2_OUTPUT, gain, 2, 2);

                copyOutput(args.sampleRate, X3_OUTPUT, gain, 3, 0);
                copyOutput(args.sampleRate, Y3_OUTPUT, gain, 3, 1);
                copyOutput(args.sampleRate, Z3_OUTPUT, gain, 3, 2);

                copyOutput(args.sampleRate, X4_OUTPUT, gain, 4, 0);
                copyOutput(args.sampleRate, Y4_OUTPUT, gain, 4, 1);
                copyOutput(args.sampleRate, Z4_OUTPUT, gain, 4, 2);
            }
        };


        struct NucleusWidget : SapphireReloadableModuleWidget
        {
            explicit NucleusWidget(NucleusModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/nucleus.svg"))
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
                addKnob(OUT_LEVEL_KNOB_PARAM, "out_level_knob");

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
        };
    }
}


Model* modelNucleus = createModel<Sapphire::Nucleus::NucleusModule, Sapphire::Nucleus::NucleusWidget>("Nucleus");
