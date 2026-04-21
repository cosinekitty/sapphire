#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

// Sapphire Split/Add/Merge for stereo and 3D vectors.
// by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace SplitAddMerge
    {
        enum ParamId
        {
            CHANNEL_COUNT_PARAM,
            ADD_TRICORDER_BUTTON_PARAM,
            PARAMS_LEN
        };

        enum InputId
        {
            POLY_INPUT,
            X_INPUT,
            Y_INPUT,
            Z_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            POLY_OUTPUT,
            X_OUTPUT,
            Y_OUTPUT,
            Z_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct SplitAddMergeModule : SapphireModule
        {
            ChannelCountQuantity *channelCountQuantity{};
            bool followInputChannelCount{};

            SplitAddMergeModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                channelCountQuantity = configChannelCount(CHANNEL_COUNT_PARAM, 3);

                configInput(POLY_INPUT, "Polyphonic (X, Y, Z)");
                configInput(X_INPUT, "X");
                configInput(Y_INPUT, "Y");
                configInput(Z_INPUT, "Z");

                configOutput(POLY_OUTPUT, "Polyphonic (X, Y, Z)");
                configOutput(X_OUTPUT, "X");
                configOutput(Y_OUTPUT, "Y");
                configOutput(Z_OUTPUT, "Z");

                configButton(ADD_TRICORDER_BUTTON_PARAM, "Insert Tricorder");

                initialize();
            }

            void initialize()
            {
                channelCountQuantity->initialize();
                followInputChannelCount = false;
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            int desiredChannelCount()
            {
                if (followInputChannelCount)
                {
                    int nc = 1;     // always output at least 1 channel

                    // Expand channel count to hold all the channels of any polyphonic input cable.
                    nc = std::max<int>(nc, inputs.at(POLY_INPUT).getChannels());

                    // If any of the monophonic ports (X, Y, Z) are connected,
                    // expand the channel count as needed to include their signal(s).
                    // No need to actually check X because we always output at least 1 channel anyway.

                    if (nc<3 && inputs.at(Z_INPUT).isConnected())
                        nc = 3;

                    if (nc<2 && inputs.at(Y_INPUT).isConnected())
                        nc = 2;

                    return nc;
                }

                return channelCountQuantity->getDesiredChannelCount();
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                json_object_set_new(root, "channels", json_integer(desiredChannelCount()));
                jsonSetBool(root, "followInputChannelCount", followInputChannelCount);
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);

                if (json_t* jChannels = json_object_get(root, "channels"); json_is_integer(jChannels))
                    if (json_int_t n = json_integer_value(jChannels); n >= 1 && n <= PORT_MAX_CHANNELS)
                        channelCountQuantity->value = static_cast<float>(n);

                jsonLoadBool(root, "followInputChannelCount", followInputChannelCount);
            }

            void process(const ProcessArgs& args) override
            {
                const int nc = desiredChannelCount();

                float px = inputs.at(POLY_INPUT).getVoltage(0);
                float py = inputs.at(POLY_INPUT).getVoltage(1);
                float pz = inputs.at(POLY_INPUT).getVoltage(2);

                float mx = inputs.at(X_INPUT).getVoltageSum();
                float my = inputs.at(Y_INPUT).getVoltageSum();
                float mz = inputs.at(Z_INPUT).getVoltageSum();

                float sx = px + mx;
                float sy = py + my;
                float sz = pz + mz;

                outputs.at(POLY_OUTPUT).setChannels(nc);
                outputs.at(POLY_OUTPUT).setVoltage(sx, 0);
                outputs.at(POLY_OUTPUT).setVoltage(sy, 1);
                outputs.at(POLY_OUTPUT).setVoltage(sz, 2);

                // If the user selects more than 3 output channels,
                // copy poly input from higher channels also.
                // There are no mono (X, Y, Z) type ports to add with.
                for (int c = 3; c < nc; ++c)
                {
                    float p = inputs.at(POLY_INPUT).getVoltage(c);
                    outputs.at(POLY_OUTPUT).setVoltage(p, c);
                }

                outputs.at(X_OUTPUT).setVoltage(sx);
                outputs.at(Y_OUTPUT).setVoltage(sy);
                outputs.at(Z_OUTPUT).setVoltage(sz);

                sendVector(sx, sy, sz, false);

                currentChannelCount = nc;   // keep the channel display widget updated
            }
        };


        struct SplitAddMergeWidget : SapphireWidget
        {
            SplitAddMergeModule* splitAddMergeModule{};

            explicit SplitAddMergeWidget(SplitAddMergeModule* module)
                : SapphireWidget("sam", asset::plugin(pluginInstance, "res/sam.svg"))
                , splitAddMergeModule(module)
            {
                setModule(module);
                addSapphireInput(POLY_INPUT, "p_input");
                addSapphireInput(X_INPUT, "x_input");
                addSapphireInput(Y_INPUT, "y_input");
                addSapphireInput(Z_INPUT, "z_input");
                addSapphireOutput(POLY_OUTPUT, "p_output");
                addSapphireOutput(X_OUTPUT, "x_output");
                addSapphireOutput(Y_OUTPUT, "y_output");
                addSapphireOutput(Z_OUTPUT, "z_output");
                addSapphireChannelDisplay("channel_display");
                addInsertTricorderButton(ADD_TRICORDER_BUTTON_PARAM);
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
                if (splitAddMergeModule)
                {
                    menu->addChild(new ChannelCountSlider(splitAddMergeModule->channelCountQuantity));

                    BoolToggleAction::AddMenuItem(
                        menu,
                        splitAddMergeModule->followInputChannelCount,
                        "Follow input channel count",
                        "channel count mode"
                    );
                }
            }
        };
    }
}

Model* modelSapphireSam = createSapphireModel<Sapphire::SplitAddMerge::SplitAddMergeModule, Sapphire::SplitAddMerge::SplitAddMergeWidget>(
    "SplitAddMerge",
    Sapphire::ExpanderRole::VectorSender
);
