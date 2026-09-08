// Sapphire Belle for VCV Rack, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <cassert>
#include <cstring>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_voice.hpp"

namespace Sapphire
{
    namespace Belle
    {
        struct BelleModule;

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


        struct GraphWidget : OpaqueWidget
        {
            BelleModule* belleModule{};

            explicit GraphWidget(BelleModule* bmod, const std::string& prefix)
                : belleModule(bmod)
            {
                ComponentLocation upperLeft  = FindComponent("belle", prefix + "_upper_left");
                ComponentLocation lowerRight = FindComponent("belle", prefix + "_lower_right");
                box.pos.x = mm2px(upperLeft.cx);
                box.pos.y = mm2px(upperLeft.cy);
                box.size.x = mm2px(lowerRight.cx - upperLeft.cx);
                box.size.y = mm2px(lowerRight.cy - upperLeft.cy);
            }

            void draw(const DrawArgs& args) override
            {
                drawBlackRectangle(args);
                OpaqueWidget::draw(args);   // in case we ever have children to draw on top
            }

            void drawBlackRectangle(const DrawArgs& args)
            {
                math::Rect r = box.zeroPos();
                nvgBeginPath(args.vg);
                nvgRect(args.vg, RECT_ARGS(r));
                nvgFillColor(args.vg, SCHEME_BLACK);
                nvgFill(args.vg);
            }
        };


        struct ModelGraphWidget : GraphWidget
        {
            const std::string fontPath;

            explicit ModelGraphWidget(BelleModule* bmod)
                : GraphWidget(bmod, "model")
                , fontPath(asset::system("res/fonts/ShareTechMono-Regular.ttf"))
            {
                initialize();
            }

            void initialize()
            {
            }

            void drawLayer(const DrawArgs& args, int layer) override;
        };


        struct EnvelopeGraphWidget : GraphWidget
        {
            explicit EnvelopeGraphWidget(BelleModule* bmod)
                : GraphWidget(bmod, "envelope")
            {
                initialize();
            }

            void initialize()
            {
            }
        };


        struct WaveformGraphWidget : GraphWidget
        {
            explicit WaveformGraphWidget(BelleModule* bmod)
                : GraphWidget(bmod, "waveform")
            {
                initialize();
            }

            void initialize()
            {
            }
        };


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

            PolyVoiceEngineBase& getCurrentEngine() const
            {
                return *polyEngineList.at(currentEngineIndex);
            }

            std::string getCurrentEngineName() const
            {
                return getCurrentEngine().getName();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void updateSelectedEngine()
            {
                const float value = params.at(MODEL_SELECT_PARAM).getValue();
                currentEngineIndex = static_cast<unsigned>(round(value));
                if (currentEngineIndex >= engineCount())
                    currentEngineIndex = 0;
            }

            void process(const ProcessArgs& args) override
            {
                updateSelectedEngine();
                auto& left  = outputs.at(AUDIO_LEFT_OUTPUT);
                auto& right = outputs.at(AUDIO_RIGHT_OUTPUT);
                if (unsigned nPolyChannels = numOutputChannels(INPUTS_LEN, 0); nPolyChannels > 0)
                {
                    PolyVoiceEngineBase& polyEngine = getCurrentEngine();

                    float gateVoltage = 0;
                    float pitchVoltage = 0;
                    float attackVoltage = 0;
                    float decayVoltage = 0;
                    float sustainVoltage = 0;
                    float releaseVoltage = 0;
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                    {
                        VoiceContext& context = polyEngine.contextArray[c];
                        nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                        nextChannelInputVoltage(pitchVoltage, PITCH_INPUT, c);
                        nextChannelInputVoltage(attackVoltage, ATTACK_CV_INPUT, c);
                        nextChannelInputVoltage(decayVoltage, DECAY_CV_INPUT, c);
                        nextChannelInputVoltage(sustainVoltage, SUSTAIN_CV_INPUT, c);
                        nextChannelInputVoltage(releaseVoltage, RELEASE_CV_INPUT, c);
                        context.setPitch(pitchVoltage);
                        context.setGateVoltage(gateVoltage);
                        context.attack  = cvGetVoltPerOctave(ATTACK_PARAM,  ATTACK_ATTEN,  attackVoltage,  -1, +1);
                        context.decay   = cvGetVoltPerOctave(DECAY_PARAM,   DECAY_ATTEN,   decayVoltage,   -1, +1);
                        context.sustain = cvGetVoltPerOctave(SUSTAIN_PARAM, SUSTAIN_ATTEN, sustainVoltage, -1, +1);
                        context.release = cvGetVoltPerOctave(RELEASE_PARAM, RELEASE_ATTEN, releaseVoltage, -1, +1);
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

                addModelDisplayWidget();
                addEnvelopeWidget();
                addWaveformWidget();
            }

            void addModelDisplayWidget()
            {
                auto widget = new ModelGraphWidget(belleModule);
                addChild(widget);
            }

            void addEnvelopeWidget()
            {
                auto widget = new EnvelopeGraphWidget(belleModule);
                addChild(widget);
            }

            void addWaveformWidget()
            {
                auto widget = new WaveformGraphWidget(belleModule);
                addChild(widget);
            }
        };


        void ModelGraphWidget::drawLayer(const DrawArgs &args, int layer)
        {
            if (belleModule)
            {
                if (layer == 1)
                {
                    DrawCenteredText(
                        args.vg,
                        fontPath,
                        14,
                        SCHEME_YELLOW,
                        box.size.x / 2,
                        box.size.y / 2,
                        belleModule->getCurrentEngineName()
                    );
                }
            }
        }
    }
}


Model* modelSapphireBelle = createSapphireModel<Sapphire::Belle::BelleModule, Sapphire::Belle::BelleWidget>(
    "Belle",
    Sapphire::ExpanderRole::None
);
