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
            PARAMS_LEN
        };

        enum InputId
        {
            POS_INPUT,
            NEG_INPUT,
            PROPORTIONAL_CV_INPUT,
            INTEGRAL_CV_INPUT,
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


        struct OpalModule : SapphireModule
        {
            FeedbackController<float> fbc[PORT_MAX_CHANNELS];

            OpalModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configInput(POS_INPUT, "Positive");
                configInput(NEG_INPUT, "Negative");
                configOutput(CONTROL_OUTPUT, "Control");
                configOutput(GATE_OUTPUT, "Gate");
                configControlGroup("Proportional response", PROPORTIONAL_PARAM, PROPORTIONAL_ATTEN, PROPORTIONAL_CV_INPUT);
                configControlGroup("Integral response", INTEGRAL_PARAM, INTEGRAL_ATTEN, INTEGRAL_CV_INPUT);
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

            void process(const ProcessArgs& args) override
            {
                int nc = std::max(inputs[POS_INPUT].getChannels(), inputs[NEG_INPUT].getChannels());
                if (nc == 0)
                {
                    outputs[CONTROL_OUTPUT].setChannels(1);
                    outputs[CONTROL_OUTPUT].setVoltage(0, 0);

                    outputs[GATE_OUTPUT].setChannels(1);
                    outputs[GATE_OUTPUT].setVoltage(0, 0);
                }
                else
                {
                    float vpos = 0;
                    float vneg = 0;
                    float cvProp = 0;
                    float cvInteg = 0;
                    outputs[CONTROL_OUTPUT].setChannels(nc);
                    for (int c = 0; c < nc; ++c)
                    {
                        nextChannelInputVoltage(vpos, POS_INPUT, c);
                        nextChannelInputVoltage(vneg, NEG_INPUT, c);

                        nextChannelInputVoltage(cvProp, PROPORTIONAL_CV_INPUT, c);
                        float prop = cvGetControlValue(PROPORTIONAL_PARAM, PROPORTIONAL_ATTEN, cvProp, -1, +1);

                        nextChannelInputVoltage(cvInteg, INTEGRAL_CV_INPUT, c);
                        float integ = cvGetControlValue(INTEGRAL_PARAM, INTEGRAL_ATTEN, cvInteg, -1, +1);

                        fbc[c].setProportionalFactor(prop);
                        fbc[c].setIntegralFactor(integ);
                        auto f = fbc[c].process(vpos-vneg, args.sampleRate);
                        outputs[CONTROL_OUTPUT].setVoltage(f.response, c);
                        outputs[GATE_OUTPUT].setVoltage(f.bounded?10:0, c);
                    }
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
                addSapphireOutput(CONTROL_OUTPUT, "control_output");
                addSapphireOutput(GATE_OUTPUT, "gate_output");
                addSapphireFlatControlGroup("proportional", PROPORTIONAL_PARAM, PROPORTIONAL_ATTEN, PROPORTIONAL_CV_INPUT);
                addSapphireFlatControlGroup("integral", INTEGRAL_PARAM, INTEGRAL_ATTEN, INTEGRAL_CV_INPUT);
            }
        };
    }
}


Model* modelSapphireOpal = createSapphireModel<Sapphire::Opal::OpalModule, Sapphire::Opal::OpalWidget>(
    "Opal",
    Sapphire::ExpanderRole::None
);
