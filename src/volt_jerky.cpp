#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "JerkCircuit.hpp"

// Volt Jerky for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


namespace VoltJerkyTypes
{
    enum ParamId
    {
        TIME_DILATION_KNOB_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        INPUTS_LEN
    };

    enum OutputId
    {
        W_OUTPUT,
        X_OUTPUT,
        Y_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId
    {
        LIGHTS_LEN
    };
}


struct VoltJerkyModule : Module
{
    Analog::JerkCircuit circuit;

    VoltJerkyModule()
        : circuit(+0.507, -0.013, +0.017)
    {
        using namespace VoltJerkyTypes;

        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configOutput(W_OUTPUT, "W");
        configOutput(X_OUTPUT, "X");
        configOutput(Y_OUTPUT, "Y");

        configParam(TIME_DILATION_KNOB_PARAM, -2, +1, 0, "Time dilation");

        initialize();
    }

    void initialize()
    {
        circuit.initialize();
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    void process(const ProcessArgs& args) override
    {
        using namespace VoltJerkyTypes;

        circuit.setTimeDilationExponent(params[TIME_DILATION_KNOB_PARAM].getValue());
        circuit.update(args.sampleRate);
        outputs[W_OUTPUT].setVoltage(circuit.wVoltage());
        outputs[X_OUTPUT].setVoltage(circuit.xVoltage());
        outputs[Y_OUTPUT].setVoltage(circuit.yVoltage());
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override
    {
        INFO("new sample rate = %f", e.sampleRate);
    }
};


struct VoltJerkyWidget : SapphireReloadableModuleWidget
{
    explicit VoltJerkyWidget(VoltJerkyModule* module)
        : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/voltjerky.svg"))
    {
        using namespace VoltJerkyTypes;

        setModule(module);

        addSapphireOutput(W_OUTPUT, "w_output");
        addSapphireOutput(X_OUTPUT, "x_output");
        addSapphireOutput(Y_OUTPUT, "y_output");

        addKnob(TIME_DILATION_KNOB_PARAM, "time_dilation_knob");

        // Load the SVG and place all controls at their correct coordinates.
        reloadPanel();
    }
};

Model* modelVoltJerky = createModel<VoltJerkyModule, VoltJerkyWidget>("VoltJerky");
