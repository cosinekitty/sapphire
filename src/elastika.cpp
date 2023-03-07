#include "plugin.hpp"
#include "elastika_engine.hpp"

// Sapphire Elastika for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


struct ElastikaModule : Module
{
    Sapphire::ElastikaEngine engine;
    DcRejectQuantity *dcRejectQuantity = nullptr;
    AgcLevelQuantity *agcLevelQuantity = nullptr;
    Sapphire::Slewer slewer;
    bool isPowerGateActive;
    bool isQuiet;
    bool enableLimiterWarning;

    enum ParamId
    {
        FRICTION_SLIDER_PARAM,
        STIFFNESS_SLIDER_PARAM,
        SPAN_SLIDER_PARAM,
        CURL_SLIDER_PARAM,
        MASS_SLIDER_PARAM,
        FRICTION_ATTEN_PARAM,
        STIFFNESS_ATTEN_PARAM,
        SPAN_ATTEN_PARAM,
        CURL_ATTEN_PARAM,
        MASS_ATTEN_PARAM,
        DRIVE_KNOB_PARAM,
        LEVEL_KNOB_PARAM,
        INPUT_TILT_KNOB_PARAM,
        OUTPUT_TILT_KNOB_PARAM,
        POWER_TOGGLE_PARAM,
        INPUT_TILT_ATTEN_PARAM,
        OUTPUT_TILT_ATTEN_PARAM,
        DC_REJECT_PARAM,
        AGC_LEVEL_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        FRICTION_CV_INPUT,
        STIFFNESS_CV_INPUT,
        SPAN_CV_INPUT,
        CURL_CV_INPUT,
        MASS_CV_INPUT,
        AUDIO_LEFT_INPUT,
        AUDIO_RIGHT_INPUT,
        POWER_GATE_INPUT,
        INPUT_TILT_CV_INPUT,
        OUTPUT_TILT_CV_INPUT,
        INPUTS_LEN
    };

    enum OutputId
    {
        AUDIO_LEFT_OUTPUT,
        AUDIO_RIGHT_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId
    {
        FRICTION_LIGHT,
        STIFFNESS_LIGHT,
        SPAN_LIGHT,
        CURL_LIGHT,
        MASS_LIGHT,
        POWER_LIGHT,
        LIGHTS_LEN
    };

    ElastikaModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(FRICTION_SLIDER_PARAM, 0, 1, 0.5, "Friction");
        configParam(STIFFNESS_SLIDER_PARAM, 0, 1, 0.5, "Stiffness");
        configParam(SPAN_SLIDER_PARAM, 0, 1, 0.5, "Spring span");
        configParam(CURL_SLIDER_PARAM, -1, +1, 0, "Magnetic field");
        configParam(MASS_SLIDER_PARAM, -1, +1, 0, "Impurity mass", "", 10, 1);

        configParam(FRICTION_ATTEN_PARAM, -1, 1, 0, "Friction attenuverter", "%", 0, 100);
        configParam(STIFFNESS_ATTEN_PARAM, -1, 1, 0, "Stiffness attenuverter", "%", 0, 100);
        configParam(SPAN_ATTEN_PARAM, -1, 1, 0, "Spring span attenuverter", "%", 0, 100);
        configParam(CURL_ATTEN_PARAM, -1, 1, 0, "Magnetic field attenuverter", "%", 0, 100);
        configParam(MASS_ATTEN_PARAM, -1, 1, 0, "Impurity mass attenuverter", "%", 0, 100);
        configParam(INPUT_TILT_ATTEN_PARAM, -1, 1, 0, "Input tilt angle attenuverter", "%", 0, 100);
        configParam(OUTPUT_TILT_ATTEN_PARAM, -1, 1, 0, "Output tilt angle attenuverter", "%", 0, 100);

        dcRejectQuantity = configParam<DcRejectQuantity>(
            DC_REJECT_PARAM,
            DC_REJECT_MIN_FREQ,
            DC_REJECT_MAX_FREQ,
            DC_REJECT_DEFAULT_FREQ,
            "DC reject cutoff",
            " Hz"
        );
        dcRejectQuantity->value = DC_REJECT_DEFAULT_FREQ;

        agcLevelQuantity = configParam<AgcLevelQuantity>(
            AGC_LEVEL_PARAM,
            AGC_LEVEL_MIN,
            AGC_DISABLE_MAX,
            AGC_LEVEL_DEFAULT,
            "Output limiter"
        );
        agcLevelQuantity->value = AGC_LEVEL_DEFAULT;

        auto driveKnob = configParam(DRIVE_KNOB_PARAM, 0, 2, 1, "Input drive", " dB", -10, 80);
        auto levelKnob = configParam(LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 80);
        configParam(INPUT_TILT_KNOB_PARAM,  0, 1, 0.5, "Input tilt angle", "°", 0, 90);
        configParam(OUTPUT_TILT_KNOB_PARAM, 0, 1, 0.5, "Output tilt angle", "°", 0, 90);

        configInput(FRICTION_CV_INPUT, "Friction CV");
        configInput(STIFFNESS_CV_INPUT, "Stiffness CV");
        configInput(SPAN_CV_INPUT, "Spring span CV");
        configInput(CURL_CV_INPUT, "Magnetic field CV");
        configInput(MASS_CV_INPUT, "Impurity mass CV");
        configInput(INPUT_TILT_CV_INPUT, "Input tilt CV");
        configInput(OUTPUT_TILT_CV_INPUT, "Output tilt CV");

        configInput(AUDIO_LEFT_INPUT, "Left audio");
        configInput(AUDIO_RIGHT_INPUT, "Right audio");
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

        configButton(POWER_TOGGLE_PARAM, "Power");
        configInput(POWER_GATE_INPUT, "Power gate");

        configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
        configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);

        for (auto& x : lights)
            x.setBrightness(0.3);

        driveKnob->randomizeEnabled = false;
        levelKnob->randomizeEnabled = false;

        initialize();
    }

    void initialize()
    {
        engine.initialize();
        engine.setDcRejectFrequency(dcRejectQuantity->value);
        dcRejectQuantity->changed = false;
        reflectAgcSlider();
        isPowerGateActive = true;
        isQuiet = false;
        slewer.enable(true);
        params[POWER_TOGGLE_PARAM].setValue(1.0f);
        enableLimiterWarning = true;
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    json_t* dataToJson() override
    {
        json_t* root = json_object();
        json_object_set_new(root, "limiterWarningLight", json_boolean(enableLimiterWarning));
        return root;
    }

    void dataFromJson(json_t* root) override
    {
        json_t *warningFlag = json_object_get(root, "limiterWarningLight");
        enableLimiterWarning = !json_is_boolean(warningFlag) || json_boolean_value(warningFlag);
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override
    {
        // We slew using a linear ramp over a time span of 1/400 of a second.
        // Round to the nearest integer number of samples for the current sample rate.
        int newRampLength = static_cast<int>(round(e.sampleRate / 400.0f));
        slewer.setRampLength(newRampLength);
    }

    float getControlValue(
        ParamId sliderId,
        ParamId attenuId,
        InputId cvInputId,
        float minSlider = 0.0f,
        float maxSlider = 1.0f)
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

    void reflectAgcSlider()
    {
        // Check for changes to the automatic gain control: its level, and whether enabled/disabled.
        if (agcLevelQuantity && agcLevelQuantity->changed)
        {
            bool enabled = agcLevelQuantity->isAgcEnabled();
            if (enabled)
                engine.setAgcLevel(agcLevelQuantity->clampedAgc());
            engine.setAgcEnabled(enabled);
            agcLevelQuantity->changed = false;
        }
    }

    void process(const ProcessArgs& args) override
    {
        using namespace Sapphire;

        // The user is allowed to turn off Elastika to reduce CPU usage.
        // Check the gate input voltage first, and debounce it.
        // If the gate is not connected, fall back to the pushbutton state.
        auto& gate = inputs[POWER_GATE_INPUT];
        if (gate.isConnected())
        {
            // If the gate input is connected, use the polyphonic sum
            // to control whether POWER is enabled or disabled.
            // Debounce the signal using hysteresis like a Schmitt trigger would.
            // See: https://vcvrack.com/manual/VoltageStandards#Triggers-and-Gates
            const float gv = gate.getVoltageSum();
            if (isPowerGateActive)
            {
                if (gv <= 0.1f)
                    isPowerGateActive = false;
            }
            else
            {
                if (gv >= 1.0f)
                    isPowerGateActive = true;
            }
        }
        else
        {
            // When no gate input is connected, allow the manual pushbutton take control.
            isPowerGateActive = (params[POWER_TOGGLE_PARAM].getValue() > 0.0f);
        }

        // Set the pushbutton illumination to track the power state,
        // whether the power state was set by the button itself or the power gate.
        lights[POWER_LIGHT].setBrightness(isPowerGateActive ? 1.0f : 0.03f);

        if (!slewer.update(isPowerGateActive))
        {
            // Output silent stereo signal without using any more CPU.
            outputs[AUDIO_LEFT_OUTPUT].setVoltage(0.0f);
            outputs[AUDIO_RIGHT_OUTPUT].setVoltage(0.0f);

            // If this is the first sample since Elastika was turned off,
            // force the mesh to go back to its starting state:
            // all balls back where they were, and cease all movement.
            if (!isQuiet)
            {
                isQuiet = true;
                engine.quiet();
            }
            return;
        }

        isQuiet = false;

        // If the user has changed the DC cutoff via the right-click menu,
        // update the output filter corner frequencies.
        if (dcRejectQuantity->changed)
        {
            engine.setDcRejectFrequency(dcRejectQuantity->value);
            dcRejectQuantity->changed = false;
        }

        reflectAgcSlider();

        // Update the mesh parameters from sliders and control voltages.

        float fric = getControlValue(FRICTION_SLIDER_PARAM, FRICTION_ATTEN_PARAM, FRICTION_CV_INPUT);
        float stif = getControlValue(STIFFNESS_SLIDER_PARAM, STIFFNESS_ATTEN_PARAM, STIFFNESS_CV_INPUT);
        float span = getControlValue(SPAN_SLIDER_PARAM, SPAN_ATTEN_PARAM, SPAN_CV_INPUT);
        float curl = getControlValue(CURL_SLIDER_PARAM, CURL_ATTEN_PARAM, CURL_CV_INPUT, -1.0f, +1.0f);
        float mass = getControlValue(MASS_SLIDER_PARAM, MASS_ATTEN_PARAM, MASS_CV_INPUT, -1.0f, +1.0f);
        float drive = params[DRIVE_KNOB_PARAM].getValue();
        float gain  = params[LEVEL_KNOB_PARAM].getValue();
        float inTilt = getControlValue(INPUT_TILT_KNOB_PARAM, INPUT_TILT_ATTEN_PARAM, INPUT_TILT_CV_INPUT);
        float outTilt = getControlValue(OUTPUT_TILT_KNOB_PARAM, OUTPUT_TILT_ATTEN_PARAM, OUTPUT_TILT_CV_INPUT);

        engine.setFriction(fric);
        engine.setStiffness(stif);
        engine.setSpan(span);
        engine.setCurl(curl);
        engine.setMass(mass);
        engine.setDrive(drive);
        engine.setGain(gain);
        engine.setInputTilt(inTilt);
        engine.setOutputTilt(outTilt);

        float leftIn = inputs[AUDIO_LEFT_INPUT].getVoltageSum();
        float rightIn = inputs[AUDIO_RIGHT_INPUT].getVoltageSum();
        float sample[2];
        engine.process(args.sampleRate, leftIn, rightIn, sample[0], sample[1]);

        // Scale ElastikaEngine's dimensionless amplitude to a +5.0V amplitude.
        sample[0] *= 5.0;
        sample[1] *= 5.0;

        // Filter the audio through the slewer to prevent clicks during power transitions.
        slewer.process(sample, 2);

        outputs[AUDIO_LEFT_OUTPUT].setVoltage(sample[0]);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sample[1]);
    }
};


class ElastikaWarningLightWidget : public LightWidget
{
private:
    ElastikaModule *elastikaModule;

    static int colorComponent(double scale, int lo, int hi)
    {
        return clamp(static_cast<int>(round(lo + scale*(hi-lo))), lo, hi);
    }

    NVGcolor warningColor(double distortion)
    {
        bool enableWarning = elastikaModule && elastikaModule->enableLimiterWarning;

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
    ElastikaWarningLightWidget(ElastikaModule *module)
        : elastikaModule(module)
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
            double distortion = elastikaModule ? elastikaModule->engine.getAgcDistortion() : 0.0;
            color = warningColor(distortion);
        }
        LightWidget::drawLayer(args, layer);
    }
};


struct ElastikaWidget : ModuleWidget
{
    ElastikaModule *elastikaModule;
    ElastikaWarningLightWidget *warningLight = nullptr;

    ElastikaWidget(ElastikaModule* module)
        : elastikaModule(module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Sliders
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec( 8.00, 46.00)), module, ElastikaModule::FRICTION_SLIDER_PARAM, ElastikaModule::FRICTION_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(19.24, 46.00)), module, ElastikaModule::STIFFNESS_SLIDER_PARAM, ElastikaModule::STIFFNESS_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(30.48, 46.00)), module, ElastikaModule::SPAN_SLIDER_PARAM, ElastikaModule::SPAN_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(41.72, 46.00)), module, ElastikaModule::CURL_SLIDER_PARAM, ElastikaModule::CURL_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(52.96, 46.00)), module, ElastikaModule::MASS_SLIDER_PARAM, ElastikaModule::MASS_LIGHT));

        // Attenuverters
        addParam(createParamCentered<Trimpot>(mm2px(Vec( 8.00, 72.00)), module, ElastikaModule::FRICTION_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(19.24, 72.00)), module, ElastikaModule::STIFFNESS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48, 72.00)), module, ElastikaModule::SPAN_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(41.72, 72.00)), module, ElastikaModule::CURL_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(52.96, 72.00)), module, ElastikaModule::MASS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec( 8.00, 12.50)), module, ElastikaModule::INPUT_TILT_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(53.00, 12.50)), module, ElastikaModule::OUTPUT_TILT_ATTEN_PARAM));

        // Drive and Level knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(14.00, 102.00)), module, ElastikaModule::DRIVE_KNOB_PARAM));
        RoundLargeBlackKnob *levelKnob = createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(46.96, 102.00)), module, ElastikaModule::LEVEL_KNOB_PARAM);
        addParam(levelKnob);

        // Superimpose a warning light on the output level knob.
        // We turn the warning light on when the limiter is distoring the output.
        warningLight = new ElastikaWarningLightWidget(module);
        warningLight->box.pos  = Vec(0.0f, 0.0f);
        warningLight->box.size = levelKnob->box.size;
        levelKnob->addChild(warningLight);

        // Tilt angle knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(19.24, 17.50)), module, ElastikaModule::INPUT_TILT_KNOB_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(41.72, 17.50)), module, ElastikaModule::OUTPUT_TILT_KNOB_PARAM));

        // CV input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 8.00, 81.74)), module, ElastikaModule::FRICTION_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(19.24, 81.74)), module, ElastikaModule::STIFFNESS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(30.48, 81.74)), module, ElastikaModule::SPAN_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(41.72, 81.74)), module, ElastikaModule::CURL_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(52.96, 81.74)), module, ElastikaModule::MASS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 8.00, 22.50)), module, ElastikaModule::INPUT_TILT_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(53.00, 22.50)), module, ElastikaModule::OUTPUT_TILT_CV_INPUT));

        // Audio input Jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 7.50, 115.00)), module, ElastikaModule::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(20.50, 115.00)), module, ElastikaModule::AUDIO_RIGHT_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(40.46, 115.00)), module, ElastikaModule::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(53.46, 115.00)), module, ElastikaModule::AUDIO_RIGHT_OUTPUT));

        // Power enable/disable
        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(30.48, 95.0)), module, ElastikaModule::POWER_TOGGLE_PARAM, ElastikaModule::POWER_LIGHT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(30.48, 104.0)), module, ElastikaModule::POWER_GATE_INPUT));
    }

    void appendContextMenu(Menu* menu) override
    {
        if (elastikaModule != nullptr)
        {
            menu->addChild(new MenuSeparator);

            if (elastikaModule->dcRejectQuantity)
            {
                // Add slider that adjusts the DC-reject filter's corner frequency.
                menu->addChild(new DcRejectSlider(elastikaModule->dcRejectQuantity));
            }

            if (elastikaModule->agcLevelQuantity)
            {
                // Add slider to adjust the AGC's level setting (5V .. 10V) or to disable AGC.
                menu->addChild(new AgcLevelSlider(elastikaModule->agcLevelQuantity));

                // Add an option to enable/disable the warning slider.
                menu->addChild(createBoolPtrMenuItem<bool>("Limiter warning light", "", &elastikaModule->enableLimiterWarning));
            }
        }
    }
};


Model* modelElastika = createModel<ElastikaModule, ElastikaWidget>("Elastika");
