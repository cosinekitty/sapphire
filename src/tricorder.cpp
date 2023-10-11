#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "tricorder.hpp"

// Tricorder for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace TricorderTypes
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
}


struct TricorderModule : Module
{
    TricorderModule()
    {
        using namespace TricorderTypes;

        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        initialize();
    }

    void initialize()
    {
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    int messageCount = 0;

    void process(const ProcessArgs& args) override
    {
        using namespace Sapphire;

        // Is a compatible module connected to the left?
        // If so, receive a triplet of voltages from it and put them in the buffer.
        if (leftExpander.module != nullptr && leftExpander.module->model == modelFrolic)
        {
            const TricorderMessage* msg = static_cast<const TricorderMessage *>(leftExpander.module->rightExpander.consumerMessage);
            if (msg != nullptr)
            {
                // Experiment to verify I understand the message passing stuff.
                if (messageCount > 0)
                {
                    --messageCount;
                }
                else
                {
                    messageCount = 44100;
                    INFO("Tricorder message: x=%f, y=%f, z=%f", msg->x, msg->y, msg->z);
                }
            }
        }
    }
};


struct TricorderWidget : SapphireReloadableModuleWidget
{
    explicit TricorderWidget(TricorderModule *module)
        : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tricorder.svg"))
    {
        using namespace TricorderTypes;

        setModule(module);

        // Load the SVG and place all controls at their correct coordinates.
        reloadPanel();
    }
};

Model *modelTricorder = createModel<TricorderModule, TricorderWidget>("Tricorder");
