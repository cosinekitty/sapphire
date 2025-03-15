#include "sapphire_feedback_controller.hpp"
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Opal
    {
        enum ParamId
        {
            PROPORTIONAL_PARAM,
            PROPORTIONAL_ATTEN,
            INTEGRAL_PARAM,
            INTEGRAL_ATTEN,
            MIN_PARAM,
            MAX_PARAM,
            HICUT_PARAM,
            HICUT_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            POS_INPUT,
            NEG_INPUT,
            PROPORTIONAL_CV_INPUT,
            INTEGRAL_CV_INPUT,
            HICUT_CV_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            CONTROL_OUTPUT,
            GATE_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        enum class GateOutputMode
        {
            GateOnBounded,
            GateOnUnbounded,
        };


        struct OpalModule : SapphireModule
        {
            FeedbackController<float> fbc[PORT_MAX_CHANNELS];
            GateOutputMode gateMode = GateOutputMode::GateOnBounded;

            OpalModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(POS_INPUT, "Positive");
                configInput(NEG_INPUT, "Negative");
                configParam(MIN_PARAM, -VoltageLimit, +VoltageLimit, -VoltageLimit, "Minimum control output voltage");
                configParam(MAX_PARAM, -VoltageLimit, +VoltageLimit, +VoltageLimit, "Maximum control output voltage");
                configOutput(CONTROL_OUTPUT, "Control");
                configOutput(GATE_OUTPUT, "Gate");
                configControlGroup("Proportional response", PROPORTIONAL_PARAM, PROPORTIONAL_ATTEN, PROPORTIONAL_CV_INPUT);
                configControlGroup("Integral response", INTEGRAL_PARAM, INTEGRAL_ATTEN, INTEGRAL_CV_INPUT);
                configControlGroup("High cutoff frequency", HICUT_PARAM, HICUT_ATTEN, HICUT_CV_INPUT, -OctaveLimit, +OctaveLimit, 0, " Hz", 2, DefaultHiCutHz);
                initialize();
            }

            void initialize()
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                    fbc[c].initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                json_object_set_new(root, "gateOutputMode", json_integer(getGateOutputMode()));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                json_t* mode = json_object_get(root, "gateOutputMode");
                if (json_is_integer(mode))
                {
                    size_t index = json_integer_value(mode);
                    setGateOutputMode(index);
                }
            }

            void process(const ProcessArgs& args) override
            {
                float vmin = params[MIN_PARAM].getValue();
                float vmax = params[MAX_PARAM].getValue();
                int nc = numOutputChannels(INPUTS_LEN, 0);
                if (nc == 0)
                {
                    float vout = std::clamp(0.0f, vmin, vmax);
                    outputs[CONTROL_OUTPUT].setChannels(1);
                    outputs[CONTROL_OUTPUT].setVoltage(0, vout);

                    outputs[GATE_OUTPUT].setChannels(1);
                    setOutputGate(false, 0);
                }
                else
                {
                    float vpos = 0;
                    float vneg = 0;
                    float cvProp = 0;
                    float cvInteg = 0;
                    float cvHiCut = 0;
                    outputs[CONTROL_OUTPUT].setChannels(nc);
                    outputs[GATE_OUTPUT].setChannels(nc);
                    for (int c = 0; c < nc; ++c)
                    {
                        nextChannelInputVoltage(vpos, POS_INPUT, c);
                        nextChannelInputVoltage(vneg, NEG_INPUT, c);

                        nextChannelInputVoltage(cvProp, PROPORTIONAL_CV_INPUT, c);
                        float prop = cvGetControlValue(PROPORTIONAL_PARAM, PROPORTIONAL_ATTEN, cvProp, -1, +1);

                        nextChannelInputVoltage(cvInteg, INTEGRAL_CV_INPUT, c);
                        float integ = cvGetControlValue(INTEGRAL_PARAM, INTEGRAL_ATTEN, cvInteg, -1, +1);

                        nextChannelInputVoltage(cvHiCut, HICUT_CV_INPUT, c);
                        float hicutVoct = cvGetVoltPerOctave(HICUT_PARAM, HICUT_ATTEN, cvHiCut, -OctaveLimit, +OctaveLimit);

                        fbc[c].setOutputRange(vmin, vmax);
                        fbc[c].setProportionalFactor(prop);
                        fbc[c].setIntegralFactor(integ);
                        fbc[c].setHiCutFrequency(hicutVoct);
                        FeedbackControllerResult<float> f = fbc[c].process(vpos-vneg, args.sampleRate);
                        outputs[CONTROL_OUTPUT].setVoltage(f.response, c);
                        setOutputGate(f.bounded, c);
                    }
                }
            }

            void setOutputGate(bool isControlOutputBounded, int channel)
            {
                float voltage;
                switch (gateMode)
                {
                case GateOutputMode::GateOnBounded:
                default:
                    voltage = isControlOutputBounded ? 10 : 0;
                    break;

                case GateOutputMode::GateOnUnbounded:
                    voltage = isControlOutputBounded ? 0 : 10;
                    break;
                }
                outputs[GATE_OUTPUT].setVoltage(voltage, channel);
            }

            size_t getGateOutputMode() const
            {
                return static_cast<size_t>(gateMode);
            }

            void setGateOutputMode(size_t index)
            {
                gateMode = static_cast<GateOutputMode>(index);
            }
        };


        void addOutputModeMenuItems(OpalModule* opalModule, Menu* menu)
        {
            std::vector<std::string> labels {
                "Gate when CTRL within min..max",   // GateOnBounded
                "Gate when CTRL outside min..max"   // GateOnUnbounded
            };

            menu->addChild(createIndexSubmenuItem(
                "Output gate mode",
                labels,
                [=]() { return opalModule->getGateOutputMode(); },
                [=](size_t mode) { opalModule->setGateOutputMode(mode); }
            ));
        }


        struct OpalGatePort : SapphirePort
        {
            void appendContextMenu(ui::Menu* menu) override
            {
                SapphirePort::appendContextMenu(menu);
                OpalModule* opalModule = dynamic_cast<OpalModule*>(module);
                if (opalModule != nullptr)
                {
                    menu->addChild(new MenuSeparator);
                    addOutputModeMenuItems(opalModule, menu);
                }
            }
        };


        struct OpalWidget : SapphireWidget
        {
            OpalModule* opalModule{};

            explicit OpalWidget(OpalModule* module)
                : SapphireWidget("opal", asset::plugin(pluginInstance, "res/opal.svg"))
                , opalModule(module)
            {
                setModule(module);
                addSapphireInput(POS_INPUT, "pos_input");
                addSapphireInput(NEG_INPUT, "neg_input");
                addSapphireFlatControlGroup("proportional", PROPORTIONAL_PARAM, PROPORTIONAL_ATTEN, PROPORTIONAL_CV_INPUT);
                addSapphireFlatControlGroup("integral", INTEGRAL_PARAM, INTEGRAL_ATTEN, INTEGRAL_CV_INPUT);
                addSapphireFlatControlGroup("hicut", HICUT_PARAM, HICUT_ATTEN, HICUT_CV_INPUT);
                addKnob<Trimpot>(MIN_PARAM, "min_knob");
                addKnob<Trimpot>(MAX_PARAM, "max_knob");
                addSapphireOutput(CONTROL_OUTPUT, "control_output");
                OpalGatePort *gatePort = addSapphireOutput<OpalGatePort>(GATE_OUTPUT, "gate_output");
                gatePort->module = module;
            }

            void appendContextMenu(Menu* menu) override
            {
                if (opalModule != nullptr)
                {
                    menu->addChild(new MenuSeparator);
                    addOutputModeMenuItems(opalModule, menu);
                }
            }
        };
    }
}


Model* modelSapphireOpal = createSapphireModel<Sapphire::Opal::OpalModule, Sapphire::Opal::OpalWidget>(
    "Opal",
    Sapphire::ExpanderRole::None
);
