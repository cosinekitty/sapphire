#include "plugin.hpp"

struct Elastika : Module
{
    enum ParamId
    {
        FRICTION_SLIDER_PARAM,
        STIFFNESS_SLIDER_PARAM,
        SPAN_SLIDER_PARAM,
        TONE_SLIDER_PARAM,
        FRICTION_ATTEN_PARAM,
        STIFFNESS_ATTEN_PARAM,
        SPAN_ATTEN_PARAM,
        TONE_ATTEN_PARAM,
        LEVEL_KNOB_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        FRICTION_CV_INPUT,
        STIFFNESS_CV_INPUT,
        SPAN_CV_INPUT,
        TONE_CV_INPUT,
        AUDIO_LEFT_INPUT,
        AUDIO_RIGHT_INPUT,
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
        TONE_LIGHT,
        LIGHTS_LEN
    };

    Elastika()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(AUDIO_LEFT_INPUT, "Left Audio");
        configInput(AUDIO_RIGHT_INPUT, "Right Audio");
        configOutput(AUDIO_LEFT_OUTPUT, "Left Audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right Audio");
    }

    void process(const ProcessArgs& args) override
    {

    }
};


struct ElastikaWidget : ModuleWidget
{
    ElastikaWidget(Elastika* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Sliders
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(10.95, 45.94)), module, Elastika::FRICTION_SLIDER_PARAM, Elastika::FRICTION_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(23.73, 45.94)), module, Elastika::STIFFNESS_SLIDER_PARAM, Elastika::STIFFNESS_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(37.04, 45.94)), module, Elastika::SPAN_SLIDER_PARAM, Elastika::SPAN_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(49.82, 45.94)), module, Elastika::TONE_SLIDER_PARAM, Elastika::TONE_LIGHT));

        // Attenuverters
        addParam(createParamCentered<Trimpot>(mm2px(Vec(10.95, 75.68)), module, Elastika::FRICTION_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(23.73, 75.68)), module, Elastika::STIFFNESS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(37.04, 75.68)), module, Elastika::SPAN_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(49.82, 75.68)), module, Elastika::TONE_ATTEN_PARAM));

        // Level knob
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(30.47, 108.94)), module, Elastika::LEVEL_KNOB_PARAM));

        // CV input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.73, 86.50)), module, Elastika::FRICTION_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(23.51, 86.50)), module, Elastika::STIFFNESS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(36.82, 86.50)), module, Elastika::SPAN_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(49.60, 86.50)), module, Elastika::TONE_CV_INPUT));

        // Audio input Jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 9.12, 108.94)), module, Elastika::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(18.53, 108.94)), module, Elastika::AUDIO_RIGHT_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(42.74, 108.94)), module, Elastika::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(52.15, 108.94)), module, Elastika::AUDIO_RIGHT_OUTPUT));
    }
};


Model* modelElastika = createModel<Elastika, ElastikaWidget>("Elastika");

