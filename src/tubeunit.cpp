#include "plugin.hpp"
#include "tubeunit_engine.hpp"

// Sapphire Tube Unit for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct TubeUnitModule : Module
{
    Sapphire::TubeUnitEngine engine[PORT_MAX_CHANNELS];
    AgcLevelQuantity *agcLevelQuantity = nullptr;
    bool enableLimiterWarning;
    int numActiveChannels;

    enum ParamId
    {
        // Large knobs for manual parameter adjustment
        AIRFLOW_PARAM,
        REFLECTION_DECAY_PARAM,
        REFLECTION_ANGLE_PARAM,
        STIFFNESS_PARAM,
        LEVEL_KNOB_PARAM,
        AGC_LEVEL_PARAM,
        BYPASS_WIDTH_PARAM,
        BYPASS_CENTER_PARAM,
        ROOT_FREQUENCY_PARAM,
        VORTEX_PARAM,

        // Attenuverter knobs
        AIRFLOW_ATTEN,
        ROOT_FREQUENCY_ATTEN,
        VORTEX_ATTEN,
        BYPASS_WIDTH_ATTEN,

        PARAMS_LEN
    };

    enum InputId
    {
        ROOT_FREQUENCY_INPUT,
        AIRFLOW_INPUT,
        VORTEX_INPUT,
        BYPASS_WIDTH_INPUT,
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
        LIGHTS_LEN
    };

    TubeUnitModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configInput(AIRFLOW_INPUT, "Airflow");
        configInput(VORTEX_INPUT, "Vortex");
        configInput(ROOT_FREQUENCY_INPUT, "Root frequency");
        configInput(BYPASS_WIDTH_INPUT, "Bypass width");

        configParam(AIRFLOW_PARAM, 0.0f, 5.0f, 1.0f, "Airflow");
        configParam(VORTEX_PARAM, 0.0f, 1.0f, 0.0f, "Vortex");
        configParam(REFLECTION_DECAY_PARAM,  0.0f, 1.0f, 0.5f, "Reflection decay");
        configParam(REFLECTION_ANGLE_PARAM, -1.0f, 1.0f, 0.1f, "Reflection angle");
        configParam(STIFFNESS_PARAM, 0.0f, 1.0f, 0.5f, "Stiffness");
        configParam(BYPASS_WIDTH_PARAM, 0.5f, 20.0f, 6.0f, "Bypass width");
        configParam(BYPASS_CENTER_PARAM, -10.0f, +10.0f, 5.0f, "Bypass center");
        configParam(ROOT_FREQUENCY_PARAM, 0.0f, 8.0f, 2.7279248f, "Root frequency", " Hz", 2, 4, 0);  // freq = (4 Hz) * (2**v)

        configParam(AIRFLOW_ATTEN, -1, 1, 0, "Airflow", "%", 0, 100);
        configParam(VORTEX_ATTEN, -1, 1, 0, "Vortex", "%", 0, 100);
        configParam(ROOT_FREQUENCY_ATTEN, -1, 1, 0, "Root frequency", "%", 0, 100);
        configParam(BYPASS_WIDTH_ATTEN, -1, 1, 0, "Bypass width", "%", 0, 100);

        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

        agcLevelQuantity = configParam<AgcLevelQuantity>(
            AGC_LEVEL_PARAM,
            AGC_LEVEL_MIN,
            AGC_DISABLE_MAX,
            AGC_LEVEL_DEFAULT,
            "Output limiter"
        );
        agcLevelQuantity->value = AGC_LEVEL_DEFAULT;

        auto levelKnob = configParam(LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 80);
        levelKnob->randomizeEnabled = false;

        initialize();
    }

    void initialize()
    {
        numActiveChannels = 0;
        enableLimiterWarning = true;

        for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
            engine[c].initialize();
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
        for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
            engine[c].setSampleRate(e.sampleRate);
    }

    void onBypass(const BypassEvent& e) override
    {
        numActiveChannels = 0;
    }

    float getControlValue(
        ParamId sliderId,
        ParamId attenuId,
        InputId cvInputId,
        int cvChannel,
        float minSlider,
        float maxSlider)
    {
        float slider = params[sliderId].getValue();
        float attenu = params[attenuId].getValue();
        float cv = inputs[cvInputId].getVoltage(cvChannel);
        // When the attenuverter is set to 100%, and the cv is +5V, we want
        // to swing a slider that is all the way down (minSlider)
        // to act like it is all the way up (maxSlider).
        // Thus we allow the complete range of control for any CV whose
        // range is [-5, +5] volts.
        return Sapphire::Clamp(slider + attenu*(cv / 5)*(maxSlider - minSlider), minSlider, maxSlider);
    }

    void process(const ProcessArgs& args) override
    {
        using namespace Sapphire;

        reflectAgcSlider();

        // Any of the inputs could have any number of channels 0..16.
        // We use simple and consistent rules to handle all possible cases:
        //
        // 1. Set the respective inputs to a default value.
        // 2. Each time we move to a new channel, if there is a corresponding input channel,
        //    update the input value.
        // 3. Otherwise keep the most recent value for the remaining channels.
        //
        // In other words, whichever input has the most channels selects the output channel count.
        // Other inputs have their final supplied value (or default value if none)
        // "normalled" to the remaining channels.

        int rootFrequencyChannels = inputs[ROOT_FREQUENCY_INPUT].getChannels();
        int airflowChannels = inputs[AIRFLOW_INPUT].getChannels();
        int vortexChannels = inputs[VORTEX_INPUT].getChannels();
        int bypassWidthChannels = inputs[BYPASS_WIDTH_INPUT].getChannels();

        numActiveChannels = std::max({
            1,
            rootFrequencyChannels,
            airflowChannels,
            vortexChannels,
            bypassWidthChannels
        });

        outputs[AUDIO_LEFT_OUTPUT ].setChannels(numActiveChannels);
        outputs[AUDIO_RIGHT_OUTPUT].setChannels(numActiveChannels);

        float rootFrequency = 4 * std::pow(2.0f, params[ROOT_FREQUENCY_PARAM].getValue());
        float vortex = params[VORTEX_PARAM].getValue();
        float airflow = params[AIRFLOW_PARAM].getValue();
        float reflectionDecay = params[REFLECTION_DECAY_PARAM].getValue();
        float reflectionAngle = M_PI * params[REFLECTION_ANGLE_PARAM].getValue();
        float stiffness = 0.005f * std::pow(10.0f, 4.0f * params[STIFFNESS_PARAM].getValue());
        float gain = params[LEVEL_KNOB_PARAM].getValue();
        float bypassWidth = params[BYPASS_WIDTH_PARAM].getValue();
        float bypassCenter = params[BYPASS_CENTER_PARAM].getValue();

        for (int c = 0; c < numActiveChannels; ++c)
        {
            if (c < rootFrequencyChannels)
                rootFrequency = 4 * std::pow(2.0f, getControlValue(ROOT_FREQUENCY_PARAM, ROOT_FREQUENCY_ATTEN, ROOT_FREQUENCY_INPUT, c, 0.0f, 8.0f));

            if (c < airflowChannels)
                airflow = getControlValue(AIRFLOW_PARAM, AIRFLOW_ATTEN, AIRFLOW_INPUT, c, 0.0f, 5.0f);

            if (c < vortexChannels)
                vortex = getControlValue(VORTEX_PARAM, VORTEX_ATTEN, VORTEX_INPUT, c, 0.0f, 1.0f);

            if (c < bypassWidthChannels)
                bypassWidth = getControlValue(BYPASS_WIDTH_PARAM, BYPASS_WIDTH_ATTEN, BYPASS_WIDTH_INPUT, c, 0.5f, 20.0f);

            engine[c].setGain(gain);
            engine[c].setAirflow(airflow);
            engine[c].setRootFrequency(rootFrequency);
            engine[c].setReflectionDecay(reflectionDecay);
            engine[c].setReflectionAngle(reflectionAngle);
            engine[c].setSpringConstant(stiffness);
            engine[c].setBypassWidth(bypassWidth);
            engine[c].setBypassCenter(bypassCenter);
            engine[c].setVortex(vortex);

            float sample[2];
            engine[c].process(sample[0], sample[1]);

            // Normalize TubeUnitEngine's dimensionless [-1, 1] output to VCV Rack's 5.0V peak amplitude.
            sample[0] *= 5.0f;
            sample[1] *= 5.0f;

            outputs[AUDIO_LEFT_OUTPUT ].setVoltage(sample[0], c);
            outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sample[1], c);
        }
    }

    void reflectAgcSlider()
    {
        // Check for changes to the automatic gain control: its level, and whether enabled/disabled.
        if (agcLevelQuantity && agcLevelQuantity->changed)
        {
            bool enabled = agcLevelQuantity->isAgcEnabled();
            for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
            {
                if (enabled)
                    engine[c].setAgcLevel(agcLevelQuantity->clampedAgc() / 5.0f);
                engine[c].setAgcEnabled(enabled);
            }
            agcLevelQuantity->changed = false;
        }
    }

    float getAgcDistortion()
    {
        // Return the maximum distortion from the engines that are actively producing output.
        float maxDistortion = 0.0f;
        for (int c = 0; c < numActiveChannels; ++c)
        {
            float distortion = engine[c].getAgcDistortion();
            if (distortion > maxDistortion)
                maxDistortion = distortion;
        }
        return maxDistortion;
    }
};


class TubeUnitWarningLightWidget : public LightWidget
{
private:
    TubeUnitModule *tubeUnitModule;

    static int colorComponent(double scale, int lo, int hi)
    {
        return clamp(static_cast<int>(round(lo + scale*(hi-lo))), lo, hi);
    }

    NVGcolor warningColor(double distortion)
    {
        bool enableWarning = tubeUnitModule && tubeUnitModule->enableLimiterWarning;

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
    TubeUnitWarningLightWidget(TubeUnitModule *module)
        : tubeUnitModule(module)
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
            double distortion = tubeUnitModule ? tubeUnitModule->getAgcDistortion() : 0.0;
            color = warningColor(distortion);
        }
        LightWidget::drawLayer(args, layer);
    }
};


inline Vec TubeUnitKnobPos(float x, float y)
{
    return mm2px(Vec(20.0f + x*26.0f, 20.0f + y*20.0f));
}


struct TubeUnitWidget : ModuleWidget
{
    TubeUnitModule *tubeUnitModule;
    TubeUnitWarningLightWidget *warningLight = nullptr;

    TubeUnitWidget(TubeUnitModule* module)
        : tubeUnitModule(module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/tubeunit.svg")));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(40.46, 115.00)), module, TubeUnitModule::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(53.46, 115.00)), module, TubeUnitModule::AUDIO_RIGHT_OUTPUT));

        // Parameter knobs
        addControlGroup(0, 0, TubeUnitModule::AIRFLOW_PARAM, TubeUnitModule::AIRFLOW_INPUT, TubeUnitModule::AIRFLOW_ATTEN);
        addControlGroup(1, 0, TubeUnitModule::VORTEX_PARAM, TubeUnitModule::VORTEX_INPUT, TubeUnitModule::VORTEX_ATTEN);
        addControlGroup(0, 1, TubeUnitModule::BYPASS_WIDTH_PARAM, TubeUnitModule::BYPASS_WIDTH_INPUT, TubeUnitModule::BYPASS_WIDTH_ATTEN);
        addParam(createParamCentered<RoundLargeBlackKnob>(TubeUnitKnobPos(1, 1), module, TubeUnitModule::BYPASS_CENTER_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(TubeUnitKnobPos(0, 2), module, TubeUnitModule::REFLECTION_DECAY_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(TubeUnitKnobPos(1, 2), module, TubeUnitModule::REFLECTION_ANGLE_PARAM));
        addControlGroup(0, 3, TubeUnitModule::ROOT_FREQUENCY_PARAM, TubeUnitModule::ROOT_FREQUENCY_INPUT, TubeUnitModule::ROOT_FREQUENCY_ATTEN);
        addParam(createParamCentered<RoundLargeBlackKnob>(TubeUnitKnobPos(1, 3), module, TubeUnitModule::STIFFNESS_PARAM));

        RoundLargeBlackKnob *levelKnob = createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(46.96, 102.00)), module, TubeUnitModule::LEVEL_KNOB_PARAM);
        addParam(levelKnob);

        // Superimpose a warning light on the output level knob.
        // We turn the warning light on when one or more of the 16 limiters are distoring the output.
        warningLight = new TubeUnitWarningLightWidget(module);
        warningLight->box.pos  = Vec(0.0f, 0.0f);
        warningLight->box.size = levelKnob->box.size;
        levelKnob->addChild(warningLight);
    }

    void addControlGroup(   // add a large control knob, CV input jack, and a small attenuverter knob
        float x,
        float y,
        TubeUnitModule::ParamId param,
        TubeUnitModule::InputId input,
        TubeUnitModule::ParamId atten)
    {
        Vec knobCenter = TubeUnitKnobPos(x, y);
        addParam(createParamCentered<RoundLargeBlackKnob>(knobCenter, tubeUnitModule, param));

        Vec attenCenter = knobCenter.plus(mm2px(Vec(-10.0, -4.0)));
        addParam(createParamCentered<Trimpot>(attenCenter, tubeUnitModule, atten));

        Vec portCenter = knobCenter.plus(mm2px(Vec(-10.0, +4.0)));
        addInput(createInputCentered<SapphirePort>(portCenter, tubeUnitModule, input));
    }

    void appendContextMenu(Menu* menu) override
    {
        if (tubeUnitModule != nullptr)
        {
            menu->addChild(new MenuSeparator);

            if (tubeUnitModule->agcLevelQuantity)
            {
                // Add slider to adjust the AGC's level setting (5V .. 10V) or to disable AGC.
                menu->addChild(new AgcLevelSlider(tubeUnitModule->agcLevelQuantity));

                // Add an option to enable/disable the warning slider.
                menu->addChild(createBoolPtrMenuItem<bool>("Limiter warning light", "", &tubeUnitModule->enableLimiterWarning));
            }
        }
    }
};

Model* modelTubeUnit = createModel<TubeUnitModule, TubeUnitWidget>("TubeUnit");
