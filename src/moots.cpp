#include "plugin.hpp"


struct Moots : Module
{
    enum ParamId
    {
        TOGGLEBUTTON1_PARAM,
        TOGGLEBUTTON2_PARAM,
        TOGGLEBUTTON3_PARAM,
        TOGGLEBUTTON4_PARAM,
        TOGGLEBUTTON5_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
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

    enum OutputId
    {
        OUTAUDIO1_OUTPUT,
        OUTAUDIO2_OUTPUT,
        OUTAUDIO3_OUTPUT,
        OUTAUDIO4_OUTPUT,
        OUTAUDIO5_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId
    {
        MOOTLIGHT1,
        MOOTLIGHT2,
        MOOTLIGHT3,
        MOOTLIGHT4,
        MOOTLIGHT5,
        LIGHTS_LEN
    };

    static const int NUM_CONTROLLERS = 5;
    static_assert(NUM_CONTROLLERS == PARAMS_LEN);
    static_assert(NUM_CONTROLLERS == OUTPUTS_LEN);
    static_assert(NUM_CONTROLLERS == LIGHTS_LEN);
    static_assert(2*NUM_CONTROLLERS == INPUTS_LEN);

    bool isGateActive[NUM_CONTROLLERS];

    Moots()
    {
        for (int i = 0; i < NUM_CONTROLLERS; ++i)
            isGateActive[i] = false;

        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configButton(TOGGLEBUTTON1_PARAM, "Moot 1");
        configButton(TOGGLEBUTTON2_PARAM, "Moot 2");
        configButton(TOGGLEBUTTON3_PARAM, "Moot 3");
        configButton(TOGGLEBUTTON4_PARAM, "Moot 4");
        configButton(TOGGLEBUTTON5_PARAM, "Moot 5");
        configInput(INAUDIO1_INPUT, "Signal 1");
        configInput(INAUDIO2_INPUT, "Signal 2");
        configInput(INAUDIO3_INPUT, "Signal 3");
        configInput(INAUDIO4_INPUT, "Signal 4");
        configInput(INAUDIO5_INPUT, "Signal 5");
        configInput(INGATE1_INPUT, "Gate 1");
        configInput(INGATE2_INPUT, "Gate 2");
        configInput(INGATE3_INPUT, "Gate 3");
        configInput(INGATE4_INPUT, "Gate 4");
        configInput(INGATE5_INPUT, "Gate 5");
        configOutput(OUTAUDIO1_OUTPUT, "Signal 1");
        configOutput(OUTAUDIO2_OUTPUT, "Signal 2");
        configOutput(OUTAUDIO3_OUTPUT, "Signal 3");
        configOutput(OUTAUDIO4_OUTPUT, "Signal 4");
        configOutput(OUTAUDIO5_OUTPUT, "Signal 5");
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);

        for (int i = 0; i < NUM_CONTROLLERS; ++i)
            isGateActive[i] = false;
    }

    void process(const ProcessArgs& args) override
    {
        float volts[PORT_MAX_CHANNELS];

        for (int i = 0; i < NUM_CONTROLLERS; ++i)
        {
            auto & gate = inputs[INGATE1_INPUT + i];

            bool active;
            if (gate.isConnected())
            {
                // If the gate input is connected, use the voltage of its first channel
                // to control whether the output is enabled or disabled.
                // Debounce the signal using hysteresis like a Schmitt trigger would.
                // See: https://vcvrack.com/manual/VoltageStandards#Triggers-and-Gates
                const float gv = gate.getVoltage(0);
                if (isGateActive[i])
                {
                    if (gv <= 0.1f)
                        isGateActive[i] = false;
                }
                else
                {
                    if (gv >= 1.0f)
                        isGateActive[i] = true;
                }
                active = isGateActive[i];
            }
            else
            {
                // When no gate input is connected, allow the manual pushbutton take control.
                active = params[TOGGLEBUTTON1_PARAM + i].getValue() > 0.0f;
            }

            lights[MOOTLIGHT1 + i].setBrightness(active ? 1.0f : 0.0f);

            auto & outp = outputs[OUTAUDIO1_OUTPUT + i];

            if (active)
            {
                auto & inp = inputs[INAUDIO1_INPUT + i];
                inp.readVoltages(volts);
                outp.channels = inp.getChannels();
                outp.writeVoltages(volts);
            }
            else
            {
                outp.channels = 0;	// force zero-channel output : shows "ghost" cable(s)
            }
        }
    }
};


struct MootsWidget : ModuleWidget
{
    MootsWidget(Moots* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/moots.svg")));

        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  17.25)), module, Moots::TOGGLEBUTTON1_PARAM, Moots::MOOTLIGHT1));
        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  38.75)), module, Moots::TOGGLEBUTTON2_PARAM, Moots::MOOTLIGHT2));
        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  60.25)), module, Moots::TOGGLEBUTTON3_PARAM, Moots::MOOTLIGHT3));
        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05,  81.75)), module, Moots::TOGGLEBUTTON4_PARAM, Moots::MOOTLIGHT4));
        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(25.05, 103.25)), module, Moots::TOGGLEBUTTON5_PARAM, Moots::MOOTLIGHT5));

        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  17.25)), module, Moots::INAUDIO1_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  38.75)), module, Moots::INAUDIO2_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  60.25)), module, Moots::INAUDIO3_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50,  81.75)), module, Moots::INAUDIO4_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.50, 103.25)), module, Moots::INAUDIO5_INPUT));

        addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  25.25)), module, Moots::INGATE1_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  46.75)), module, Moots::INGATE2_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  68.25)), module, Moots::INGATE3_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05,  89.75)), module, Moots::INGATE4_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(25.05, 111.25)), module, Moots::INGATE5_INPUT));

        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  17.25)), module, Moots::OUTAUDIO1_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  38.75)), module, Moots::OUTAUDIO2_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  60.25)), module, Moots::OUTAUDIO3_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60,  81.75)), module, Moots::OUTAUDIO4_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(39.60, 103.25)), module, Moots::OUTAUDIO5_OUTPUT));
    }
};


Model* modelMoots = createModel<Moots, MootsWidget>("Moots");
