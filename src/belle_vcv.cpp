// Sapphire Belle for VCV Rack, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <cassert>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_voice.hpp"

namespace Sapphire
{
    namespace Belle
    {
        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            PITCH_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_LEFT_OUTPUT,
            AUDIO_RIGHT_OUTPUT,

            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        constexpr unsigned DefaultEngineIndex = 0;


        struct BelleModule : SapphireModule
        {
            PolyVoiceEngine<SineEngine> polySine;
            std::vector<PolyVoiceEngineBase*> polyEngineList;
            unsigned currentEngineIndex{};

            BelleModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                registerVoiceEngines();

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(GATE_INPUT, "Gate");
                configInput(PITCH_INPUT, "Pitch (V/OCT)");

                configOutput(AUDIO_LEFT_OUTPUT,  "Left audio");
                configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

                initialize();
            }

            void registerVoiceEngines()
            {
                polyEngineList.push_back(&polySine);
            }

            void initialize()
            {
                currentEngineIndex = DefaultEngineIndex;
            }

            PolyVoiceEngineBase& getCurrentEngine()
            {
                return *polyEngineList.at(currentEngineIndex);
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                if (unsigned nPolyChannels = numOutputChannels(INPUTS_LEN, 0); nPolyChannels > 0)
                {
                    PolyVoiceEngineBase& polyEngine = getCurrentEngine();
                    PolyStereoFrame frame = polyEngine.process(args.sampleRate, nPolyChannels);

                    auto& left = outputs.at(AUDIO_LEFT_OUTPUT);
                    left.setChannels(nPolyChannels);
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                        left.setVoltage(frame.poly[c].sample[0], c);

                    auto& right = outputs.at(AUDIO_RIGHT_OUTPUT);
                    right.setChannels(nPolyChannels);
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                        right.setVoltage(frame.poly[c].sample[1], c);
                }
            }
        };


        struct BelleWidget : SapphireWidget
        {
            BelleModule* belleModule{};

            explicit BelleWidget(BelleModule* module)
                : SapphireWidget("belle", asset::plugin(pluginInstance, "res/belle.svg"))
                , belleModule(module)
            {
                setModule(module);
                addSapphireInput(GATE_INPUT, "gate_input");
                addSapphireInput(PITCH_INPUT, "pitch_input");
                addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
            }
        };
    }
}


Model* modelSapphireBelle = createSapphireModel<Sapphire::Belle::BelleModule, Sapphire::Belle::BelleWidget>(
    "Belle",
    Sapphire::ExpanderRole::None
);
