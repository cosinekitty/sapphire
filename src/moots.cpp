#include "plugin.hpp"


struct Moots : Module {
	enum ParamId {
		TOGGLEBUTTON1_PARAM,
		TOGGLEBUTTON2_PARAM,
		TOGGLEBUTTON3_PARAM,
		TOGGLEBUTTON4_PARAM,
		TOGGLEBUTTON5_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INAUDIO1_INPUT,
		INAUDIO2_INPUT,
		INAUDIO3_INPUT,
		INAUDIO4_INPUT,
		INAUDIO5_INPUT,
		INGATE1_INPUT,
		INGATE2_INPUT,
		INGATE3_INPUT,
		INGATE4_INPUT,
		INGATE5_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTAUDIO1_OUTPUT,
		OUTAUDIO2_OUTPUT,
		OUTAUDIO3_OUTPUT,
		OUTAUDIO4_OUTPUT,
		OUTAUDIO5_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		MOOTLIGHT1,
		MOOTLIGHT2,
		MOOTLIGHT3,
		MOOTLIGHT4,
		MOOTLIGHT5,
		LIGHTS_LEN
	};

	Moots() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(TOGGLEBUTTON1_PARAM, "Moot 1");
		configButton(TOGGLEBUTTON2_PARAM, "Moot 2");
		configButton(TOGGLEBUTTON3_PARAM, "Moot 3");
		configButton(TOGGLEBUTTON4_PARAM, "Moot 4");
		configButton(TOGGLEBUTTON5_PARAM, "Moot 5");
		configInput(INAUDIO1_INPUT, "");
		configInput(INAUDIO2_INPUT, "");
		configInput(INAUDIO3_INPUT, "");
		configInput(INAUDIO4_INPUT, "");
		configInput(INAUDIO5_INPUT, "");
		configInput(INGATE1_INPUT, "");
		configInput(INGATE2_INPUT, "");
		configInput(INGATE3_INPUT, "");
		configInput(INGATE4_INPUT, "");
		configInput(INGATE5_INPUT, "");
		configOutput(OUTAUDIO1_OUTPUT, "");
		configOutput(OUTAUDIO2_OUTPUT, "");
		configOutput(OUTAUDIO3_OUTPUT, "");
		configOutput(OUTAUDIO4_OUTPUT, "");
		configOutput(OUTAUDIO5_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct MootsWidget : ModuleWidget {
	MootsWidget(Moots* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/moots.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  17.25)), module, Moots::TOGGLEBUTTON1_PARAM, Moots::MOOTLIGHT1));
		addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  38.75)), module, Moots::TOGGLEBUTTON2_PARAM, Moots::MOOTLIGHT2));
		addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  60.25)), module, Moots::TOGGLEBUTTON3_PARAM, Moots::MOOTLIGHT3));
		addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  81.75)), module, Moots::TOGGLEBUTTON4_PARAM, Moots::MOOTLIGHT4));
		addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05, 103.25)), module, Moots::TOGGLEBUTTON5_PARAM, Moots::MOOTLIGHT5));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.50,  17.25)), module, Moots::INAUDIO1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.50,  38.75)), module, Moots::INAUDIO2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.50,  60.25)), module, Moots::INAUDIO3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.50,  81.75)), module, Moots::INAUDIO4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.50, 103.25)), module, Moots::INAUDIO5_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.05,  25.25)), module, Moots::INGATE1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.05,  46.75)), module, Moots::INGATE2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.05,  68.25)), module, Moots::INGATE3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.05,  89.75)), module, Moots::INGATE4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.05, 111.25)), module, Moots::INGATE5_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.60,  17.25)), module, Moots::OUTAUDIO1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.60,  38.75)), module, Moots::OUTAUDIO2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.60,  60.25)), module, Moots::OUTAUDIO3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.60,  81.75)), module, Moots::OUTAUDIO4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.60, 103.25)), module, Moots::OUTAUDIO5_OUTPUT));
	}
};


Model* modelMoots = createModel<Moots, MootsWidget>("Moots");
