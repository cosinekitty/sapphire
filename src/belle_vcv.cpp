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
        constexpr int OctaveRange = 4;            // +/- octave range around default frequency

        enum ParamId
        {
            FREQ_PARAM,
            FREQ_ATTEN,
            OCT_PARAM,
            OCT_ATTEN,
            ATTACK_PARAM,
            ATTACK_ATTEN,
            DECAY_PARAM,
            DECAY_ATTEN,
            SUSTAIN_PARAM,
            SUSTAIN_ATTEN,
            RELEASE_PARAM,
            RELEASE_ATTEN,
            ENUMS(MOD_PARAM_0, 4),
            ENUMS(MOD_ATTEN_0, 4),
            MODEL_SELECT_PARAM,

            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            PITCH_INPUT,
            FREQ_CV_INPUT,
            OCT_CV_INPUT,
            ATTACK_CV_INPUT,
            DECAY_CV_INPUT,
            SUSTAIN_CV_INPUT,
            RELEASE_CV_INPUT,

            ENUMS(MOD_CV_INPUT_0, 4),

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
            PolyVoiceEngine<TriangleEngine> polyTriangle;
            PolyVoiceEngine<SawEngine> polySaw;
            PolyVoiceEngine<SquareEngine> polySquare;
            std::vector<PolyVoiceEngineBase*> polyEngineList;
            unsigned currentEngineIndex{};

            BelleModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                polyEngineList.push_back(&polySine);
                polyEngineList.push_back(&polyTriangle);
                polyEngineList.push_back(&polySaw);
                polyEngineList.push_back(&polySquare);

                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(GATE_INPUT, "Gate");
                configInput(PITCH_INPUT, "Pitch (V/OCT)");

                configOutput(AUDIO_LEFT_OUTPUT,  "Left audio");
                configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

                configControlGroup("Frequency", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT, -OctaveRange, +OctaveRange, 0);
                configControlGroup("Octave", OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT, -OctaveRange, +OctaveRange, 0);
                paramQuantities.at(OCT_PARAM)->snapEnabled = true;

                configControlGroup("Attack", ATTACK_PARAM, ATTACK_ATTEN, ATTACK_CV_INPUT);
                configControlGroup("Decay", DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                configControlGroup("Sustain", SUSTAIN_PARAM, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT);
                configControlGroup("Release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);

                for (int m = 0; m < 4; ++m)
                    configControlGroup("", MOD_PARAM_0 + m, MOD_ATTEN_0 + m, MOD_CV_INPUT_0 + m);

                configParam(MODEL_SELECT_PARAM, 0, engineCount()-1, 0, "Model");
                paramQuantities.at(MODEL_SELECT_PARAM)->snapEnabled = true;

                initialize();
            }

            unsigned engineCount() const
            {
                return polyEngineList.size();
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
                auto& left  = outputs.at(AUDIO_LEFT_OUTPUT);
                auto& right = outputs.at(AUDIO_RIGHT_OUTPUT);
                if (unsigned nPolyChannels = numOutputChannels(INPUTS_LEN, 0); nPolyChannels > 0)
                {
                    PolyVoiceEngineBase& polyEngine = getCurrentEngine();

                    float gateVoltage = 0;
                    float pitchVoltage = 0;
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                    {
                        VoiceContext& context = polyEngine.contextArray[c];
                        nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                        nextChannelInputVoltage(pitchVoltage, PITCH_INPUT, c);
                        context.setPitch(pitchVoltage);
                        context.setGateVoltage(gateVoltage);
                    }

                    PolyStereoFrame frame = polyEngine.process(args.sampleRate, nPolyChannels);

                    left.setChannels(nPolyChannels);
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                        left.setVoltage(frame.poly[c].sample[0], c);

                    right.setChannels(nPolyChannels);
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                        right.setVoltage(frame.poly[c].sample[1], c);
                }
                else
                {
                    left.setChannels(1);
                    left.setVoltage(0, 0);

                    right.setChannels(1);
                    right.setVoltage(0, 0);
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
                addKnob(MODEL_SELECT_PARAM, "model_select");
                addSapphireInput(GATE_INPUT, "gate_input");
                addSapphireInput(PITCH_INPUT, "pitch_input");
                addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                addSnapVoctFlatControlGroup("freq", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
                addSnapVoctFlatControlGroup("oct", OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT);
                addSapphireFlatControlGroup("attack", ATTACK_PARAM, ATTACK_ATTEN, ATTACK_CV_INPUT);
                addSapphireFlatControlGroup("decay", DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                addSapphireFlatControlGroup("sustain", SUSTAIN_PARAM, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT);
                addSapphireFlatControlGroup("release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);

                for (int m = 0; m < 4; ++m)
                {
                    addSapphireFlatControlGroup(
                        "mod" + std::to_string(m),
                        MOD_PARAM_0 + m,
                        MOD_ATTEN_0 + m,
                        MOD_CV_INPUT_0 + m
                    );
                }
            }
        };
    }
}


Model* modelSapphireBelle = createSapphireModel<Sapphire::Belle::BelleModule, Sapphire::Belle::BelleWidget>(
    "Belle",
    Sapphire::ExpanderRole::None
);
