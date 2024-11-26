#include "sapphire_chaos_module.hpp"

// Chaos Operators (Chaops)
// A left-expander for Sapphire chaos modules like Frolic, Glee, Lark.
// Adds extra functionality for controlling the internal behavior
// of the chaotic oscillator algorithms.

namespace Sapphire
{
    namespace ChaosOperators
    {
        enum ParamId
        {
            TIME_PARAM,
            TIME_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            TIME_CV_INPUT,
            INPUTS_LEN
        };

        enum OutputId
        {
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct ChaopsModule : SapphireModule
        {
            Sender sender;
            Message message;

            ChaopsModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
                , sender(*this)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                configParam(TIME_PARAM, -1, +1, +1, "Time flow");
                configParam(TIME_ATTEN, -1, +1, 0, "Time flow attenuverter", "%", 0, 100);
                configInput(TIME_CV_INPUT, "Time flow CV");
                initialize();
            }

            void initialize()
            {
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                if (sender.isReceiverConnectedOnRight())
                {
                    message.timeFactor = 0.1;     // FIXFIXFIX read value from control group

                    for (int c = 0; c < 16; ++c)
                    {
                        message.channel[c].x = 0;
                        message.channel[c].y = 0;
                        message.channel[c].z = 0;
                        message.channel[c].recall = false;
                        message.channel[c].store  = false;
                    }

                    sender.send(message);
                }
            }
        };


        struct ChaopsWidget : SapphireWidget
        {
            ChaopsModule* chaopsModule;

            explicit ChaopsWidget(ChaopsModule* module)
                : SapphireWidget("chaops", asset::plugin(pluginInstance, "res/chaops.svg"))
                , chaopsModule(module)
            {
                setModule(module);
                addSapphireControlGroup("time", TIME_PARAM, TIME_ATTEN, TIME_CV_INPUT);
            }
        };
    }
}

Model* modelSapphireChaops = createSapphireModel<Sapphire::ChaosOperators::ChaopsModule, Sapphire::ChaosOperators::ChaopsWidget>(
    "Chaops",
    Sapphire::ExpanderRole::ChaosOpSender
);
