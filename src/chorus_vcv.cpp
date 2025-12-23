#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "chorus_engine.hpp"

namespace Sapphire
{
    namespace Chorus
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
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


        struct ChorusModule : SapphireModule
        {
            Engine engine;

            ChorusModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void initialize()
            {
            }

            void process(const ProcessArgs& args) override
            {
            }
        };


        struct ChorusWidget : SapphireWidget
        {
            ChorusModule* chorusModule{};

            explicit ChorusWidget(ChorusModule* module)
                : SapphireWidget("chorus", asset::plugin(pluginInstance, "res/chorus.svg"))
                , chorusModule(module)
            {
                setModule(module);
            }
        };
    }
}


Model *modelSapphireChorus = createSapphireModel<Sapphire::Chorus::ChorusModule, Sapphire::Chorus::ChorusWidget>(
    "Chorus",
    Sapphire::ExpanderRole::None
);
