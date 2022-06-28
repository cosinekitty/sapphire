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
		INGATE1_INPUT,
		INAUDIO2_INPUT,
		INGATE2_INPUT,
		INAUDIO3_INPUT,
		INGATE3_INPUT,
		INAUDIO4_INPUT,
		INGATE4_INPUT,
		INAUDIO5_INPUT,
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
		LIGHTS_LEN
	};

	Moots() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLEBUTTON1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TOGGLEBUTTON2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TOGGLEBUTTON3_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TOGGLEBUTTON4_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TOGGLEBUTTON5_PARAM, 0.f, 1.f, 0.f, "");
		configInput(INAUDIO1_INPUT, "");
		configInput(INGATE1_INPUT, "");
		configInput(INAUDIO2_INPUT, "");
		configInput(INGATE2_INPUT, "");
		configInput(INAUDIO3_INPUT, "");
		configInput(INGATE3_INPUT, "");
		configInput(INAUDIO4_INPUT, "");
		configInput(INGATE4_INPUT, "");
		configInput(INAUDIO5_INPUT, "");
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
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Moots.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.051, 17.252)), module, Moots::TOGGLEBUTTON1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.051, 38.863)), module, Moots::TOGGLEBUTTON2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.051, 60.298)), module, Moots::TOGGLEBUTTON3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.051, 81.869)), module, Moots::TOGGLEBUTTON4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.051, 103.251)), module, Moots::TOGGLEBUTTON5_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.554, 17.347)), module, Moots::INAUDIO1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.051, 25.026)), module, Moots::INGATE1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.554, 38.958)), module, Moots::INAUDIO2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.051, 46.608)), module, Moots::INGATE2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.554, 60.393)), module, Moots::INAUDIO3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.051, 68.015)), module, Moots::INGATE3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.554, 81.964)), module, Moots::INAUDIO4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.051, 89.245)), module, Moots::INGATE4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.554, 103.346)), module, Moots::INAUDIO5_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.051, 110.732)), module, Moots::INGATE5_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.883, 17.171)), module, Moots::OUTAUDIO1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.883, 38.863)), module, Moots::OUTAUDIO2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.883, 60.298)), module, Moots::OUTAUDIO3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.883, 81.869)), module, Moots::OUTAUDIO4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.883, 103.251)), module, Moots::OUTAUDIO5_OUTPUT));
	}
};


Model* modelMoots = createModel<Moots, MootsWidget>("Moots");
