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
            AdsrVoiceEngine<SineEngine> polySine{"sine", "Detune", "Sin1", "Sin2", "Sin3"};
            AdsrVoiceEngine<TriangleEngine> polyTriangle{"triangle", "Detune", "Tri1", "Tri2", "Tri3"};
            AdsrVoiceEngine<SawEngine> polySaw{"saw", "Detune", "Saw1", "Saw2", "Saw3"};
            AdsrVoiceEngine<SquareEngine> polySquare{"square", "Detune", "Sqr1", "Sqr2", "Sqr3"};

            std::vector<PolyStereoVoice*> polyEngineList;
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
                configControlGroup("Sustain", SUSTAIN_PARAM, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT, 0, 1, 0.5, "%", 0, 100);
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

            PolyStereoVoice& getCurrentEngine() const
            {
                return *polyEngineList.at(currentEngineIndex);
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
                    PolyStereoVoice& polyEngine = getCurrentEngine();

                    float gateVoltage = 0;
                    float pitchVoltage = 0;
                    float freqVoltage = 0;
                    float octaveVoltage = 0;
                    float attackVoltage = 0;
                    float decayVoltage = 0;
                    float sustainVoltage = 0;
                    float releaseVoltage = 0;
                    float modVoltage[NUM_DYNAMIC_PARAMS]{};
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                    {
                        VoiceContext& context = polyEngine.contextArray[c];
                        nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                        nextChannelInputVoltage(pitchVoltage, PITCH_INPUT, c);
                        nextChannelInputVoltage(freqVoltage, FREQ_CV_INPUT, c);
                        nextChannelInputVoltage(octaveVoltage, OCT_CV_INPUT, c);
                        nextChannelInputVoltage(attackVoltage, ATTACK_CV_INPUT, c);
                        nextChannelInputVoltage(decayVoltage, DECAY_CV_INPUT, c);
                        nextChannelInputVoltage(sustainVoltage, SUSTAIN_CV_INPUT, c);
                        nextChannelInputVoltage(releaseVoltage, RELEASE_CV_INPUT, c);
                        for (unsigned m = 0; m < NUM_DYNAMIC_PARAMS; ++m)
                            nextChannelInputVoltage(modVoltage[m], MOD_CV_INPUT_0+m, c);

                        float freq = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, freqVoltage, -OctaveRange, +OctaveRange);
                        float oct  = cvGetVoltPerOctave(OCT_PARAM, OCT_ATTEN, octaveVoltage, -OctaveRange, +OctaveRange);
                        context.setPitch(pitchVoltage + freq + oct);
                        context.setGateVoltage(gateVoltage);
                        context.attack  = cvGetVoltPerOctave(ATTACK_PARAM,  ATTACK_ATTEN,  attackVoltage,  -1, +1);
                        context.decay   = cvGetVoltPerOctave(DECAY_PARAM,   DECAY_ATTEN,   decayVoltage,   -1, +1);
                        context.sustain = cvGetVoltPerOctave(SUSTAIN_PARAM, SUSTAIN_ATTEN, sustainVoltage,  0, +1);
                        context.release = cvGetVoltPerOctave(RELEASE_PARAM, RELEASE_ATTEN, releaseVoltage, -1, +1);
                        for (unsigned m = 0; m < NUM_DYNAMIC_PARAMS; ++m)
                            context.mod[m] = cvGetVoltPerOctave(MOD_PARAM_0+m, MOD_ATTEN_0+m, modVoltage[m], -1, +1);
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

            void updateControls()
            {
                const PolyStereoVoice& engine = getCurrentEngine();
                updateDynamicControlGroup(MOD_PARAM_0+0, MOD_ATTEN_0+0, MOD_CV_INPUT_0+0, engine.mod0);
                updateDynamicControlGroup(MOD_PARAM_0+1, MOD_ATTEN_0+1, MOD_CV_INPUT_0+1, engine.mod1);
                updateDynamicControlGroup(MOD_PARAM_0+2, MOD_ATTEN_0+2, MOD_CV_INPUT_0+2, engine.mod2);
                updateDynamicControlGroup(MOD_PARAM_0+3, MOD_ATTEN_0+3, MOD_CV_INPUT_0+3, engine.mod3);
            }

            void updateDynamicControlGroup(int paramId, int attenId, int inputId, const std::string& name)
            {
                getParamQuantity(paramId)->name = name;
                getParamQuantity(attenId)->name = name + " attenuverter";
                getInputInfo(inputId)->name = name + " CV";
            }
        };


        struct BelleOverlayInfo
        {
            std::string engineName;
            SvgOverlay* layer{};

            explicit BelleOverlayInfo(const std::string &_engineName, SvgOverlay* _layer)
                : engineName(_engineName)
                , layer(_layer)
                {}
        };


        struct BelleWidget : SapphireWidget
        {
            BelleModule* belleModule{};
            std::vector<BelleOverlayInfo> labelOverlayList;

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
                addOverlays();
            }

            void step() override
            {
                SapphireWidget::step();
                if (belleModule)
                {
                    const std::string& currentEngineName = belleModule->getCurrentEngine().name;
                    for (const BelleOverlayInfo& info : labelOverlayList)
                        info.layer->setVisible(info.engineName == currentEngineName);

                    belleModule->updateControls();
                }
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

            void addOverlays()
            {
                const std::vector<std::string> engineNameList
                {
                    "sine",
                    "triangle",
                    "saw",
                    "square",
                };

                for (const std::string& engine : engineNameList)
                {
                    SvgOverlay* layer = SvgOverlay::Load("res/belle_overlay_" + engine + ".svg");
                    addChild(layer);
                    layer->hide();
                    labelOverlayList.push_back(BelleOverlayInfo(engine, layer));
                }
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
                        belleModule->getCurrentEngine().name
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
