// Sapphire Belle for VCV Rack, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <cassert>
#include <cstring>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_voice.hpp"
#include "chaos_fountain.hpp"

namespace Sapphire
{
    namespace Belle
    {
        struct BelleModule;

        constexpr int OctaveRange = 4;            // +/- octave range around default frequency

        constexpr unsigned nChaoticSignals = 11;
        using fountain_t = ChaosFountain<nChaoticSignals>;
        using batch_t = ChaosBatch<nChaoticSignals>;

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
            CHAOS_SPEED_PARAM,
            CHAOS_SPEED_ATTEN,
            CHAOS_LEVEL_PARAM,
            CHAOS_LEVEL_ATTEN,
            CHAOS_STEREO_BUTTON_PARAM,
            CHAOS_RANDOMIZE_BUTTON_PARAM,
            CHAOS_FREEZE_BUTTON_PARAM,
            CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM,
            SAMPLE_HOLD_BUTTON_PARAM,

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
            CHAOS_SPEED_CV_INPUT,
            CHAOS_LEVEL_CV_INPUT,

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


        constexpr unsigned DefaultEngineIndex = 2;


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
            fountain_t fountain{rack::random::u64()};
            float speedChaos{};

            AdsrVoiceEngine<SineEngine> polySine{"sine", "Detune", "Sin1", "Sin2", "Sin3"};
            AdsrVoiceEngine<TriangleEngine> polyTriangle{"triangle", "Detune", "Tri1", "Tri2", "Tri3"};
            AdsrVoiceEngine<SawEngine> polySaw{"saw", "Detune", "Saw1", "Saw2", "Saw3"};
            AdsrVoiceEngine<SquareEngine> polySquare{"square", "Detune", "PWM", "Sqr2", "Sqr3"};

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
                configButton(SAMPLE_HOLD_BUTTON_PARAM, "Sample and hold");

                configControlGroup("Frequency", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT, -OctaveRange, +OctaveRange, 0);
                configControlGroup("Octave", OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT, -OctaveRange, +OctaveRange, 0);
                paramQuantities.at(OCT_PARAM)->snapEnabled = true;

                configControlGroup("Attack", ATTACK_PARAM, ATTACK_ATTEN, ATTACK_CV_INPUT);
                configControlGroup("Decay", DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                configControlGroup("Sustain", SUSTAIN_PARAM, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT, 0, 1, 0.5, "%", 0, 100);
                configControlGroup("Release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);

                for (int m = 0; m < 4; ++m)
                    configControlGroup("", MOD_PARAM_0 + m, MOD_ATTEN_0 + m, MOD_CV_INPUT_0 + m);

                configParam(MODEL_SELECT_PARAM, 0, engineCount()-1, DefaultEngineIndex, "Model");
                paramQuantities.at(MODEL_SELECT_PARAM)->snapEnabled = true;

                configChaosBox();

                initialize();
            }

            void configChaosBox()
            {
                configControlGroup("Chaos speed", CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT, -ChaosOctaveRange, +ChaosOctaveRange);
                configControlGroup("Chaos level", CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
#ifdef ENABLE_CHAOS_STEREO_BUTTON
                configButton(CHAOS_STEREO_BUTTON_PARAM);
#endif
                configButton(CHAOS_RANDOMIZE_BUTTON_PARAM, "Randomize chaotic CV");
                configButton(CHAOS_FREEZE_BUTTON_PARAM);
                configButton(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM);

                attenuverterChaosOptIn(FREQ_ATTEN);
                attenuverterChaosOptIn(OCT_ATTEN);
                attenuverterChaosOptIn(ATTACK_ATTEN);
                attenuverterChaosOptIn(DECAY_ATTEN);
                attenuverterChaosOptIn(SUSTAIN_ATTEN);
                attenuverterChaosOptIn(RELEASE_ATTEN);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 0);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 1);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 2);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 3);
                attenuverterChaosOptIn(CHAOS_SPEED_ATTEN);
            }

            bool shouldDisplayChaosVoltages() override
            {
                return params.at(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM).getValue() > 0.5f;
            }

            unsigned engineCount() const
            {
                return polyEngineList.size();
            }

            void initialize()
            {
                fountain.reset();
                speedChaos = 0;
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

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                jsonSaveSeed(root, "chaosFountainSeed", fountain.getSeed());
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                if (uint64_t seed = jsonLoadOrGenerateSeed(root, "chaosFountainSeed"))
                    fountain.reset(seed);
            }

            void updateSelectedEngine()
            {
                const float value = params.at(MODEL_SELECT_PARAM).getValue();
                currentEngineIndex = static_cast<unsigned>(round(value));
                if (currentEngineIndex >= engineCount())
                    currentEngineIndex = 0;
            }

            void updateChaos(float sampleRateHz, batch_t& batch)
            {
                const float speedKnob = getControlValueChaos(
                    CHAOS_SPEED_PARAM,
                    CHAOS_SPEED_ATTEN,
                    CHAOS_SPEED_CV_INPUT,
                    speedChaos,
                    -ChaosOctaveRange,
                    +ChaosOctaveRange
                );

                const bool isChaosLevelZero =
                    (params.at(CHAOS_LEVEL_PARAM).getValue() == 0) &&
                    (params.at(CHAOS_LEVEL_ATTEN).getValue() == 0);

                const bool isChaosFreezeButtonPressed =
                    (params.at(CHAOS_FREEZE_BUTTON_PARAM).getValue() == 1);

                const bool isChaosFrozen = isChaosLevelZero || isChaosFreezeButtonPressed;

                const float levelKnob = Cube(getControlValueVoltPerOctave(
                    CHAOS_LEVEL_PARAM,
                    CHAOS_LEVEL_ATTEN,
                    CHAOS_LEVEL_CV_INPUT,
                    0,
                    2
                ));

                if (!isChaosFrozen)
                {
                    const float dt = SimulationTimeIncrement(sampleRateHz, speedKnob);
                    fountain.update(dt);
                }

                batch = fountain.getBatch(levelKnob);
                reportChaosMono(FREQ_ATTEN,         batch( 0));
                reportChaosMono(OCT_ATTEN,          batch( 1));
                reportChaosMono(ATTACK_ATTEN,       batch( 2));
                reportChaosMono(DECAY_ATTEN,        batch( 3));
                reportChaosMono(SUSTAIN_ATTEN,      batch( 4));
                reportChaosMono(RELEASE_ATTEN,      batch( 5));
                reportChaosMono(MOD_ATTEN_0+0,      batch( 6));
                reportChaosMono(MOD_ATTEN_0+1,      batch( 7));
                reportChaosMono(MOD_ATTEN_0+2,      batch( 8));
                reportChaosMono(MOD_ATTEN_0+3,      batch( 9));
                reportChaosMono(CHAOS_SPEED_ATTEN,  batch(10));
            }

            void process(const ProcessArgs& args) override
            {
                updateSelectedEngine();

                auto& left  = outputs.at(AUDIO_LEFT_OUTPUT);
                auto& right = outputs.at(AUDIO_RIGHT_OUTPUT);
                if (unsigned nPolyChannels = numOutputChannels(INPUTS_LEN, 0); nPolyChannels > 0)
                {
                    PolyStereoVoice& polyEngine = getCurrentEngine();

                    batch_t batch;
                    updateChaos(args.sampleRate, batch);
                    speedChaos = batch(10);

                    const bool isSampleHoldEnabled = (params.at(SAMPLE_HOLD_BUTTON_PARAM).getValue() > 0.5f);

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
                        nextVoltageOrChaosSignal(freqVoltage, FREQ_CV_INPUT, c,       batch(0));
                        nextVoltageOrChaosSignal(octaveVoltage, OCT_CV_INPUT, c,      batch(1));
                        nextVoltageOrChaosSignal(attackVoltage, ATTACK_CV_INPUT, c,   batch(2));
                        nextVoltageOrChaosSignal(decayVoltage, DECAY_CV_INPUT, c,     batch(3));
                        nextVoltageOrChaosSignal(sustainVoltage, SUSTAIN_CV_INPUT, c, batch(4));
                        nextVoltageOrChaosSignal(releaseVoltage, RELEASE_CV_INPUT, c, batch(5));
                        for (unsigned m = 0; m < NUM_DYNAMIC_PARAMS; ++m)
                            nextVoltageOrChaosSignal(modVoltage[m], MOD_CV_INPUT_0+m, c, batch(6+m));

                        float freq = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, freqVoltage, -OctaveRange, +OctaveRange);
                        float oct = std::round(cvGetVoltPerOctave(OCT_PARAM, OCT_ATTEN, octaveVoltage, -OctaveRange, +OctaveRange));
                        context.updateGatePitch(gateVoltage, pitchVoltage + freq + oct, isSampleHoldEnabled);
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

#ifdef ENABLE_CHAOS_STEREO_BUTTON
                updateToggleButtonTooltip(CHAOS_STEREO_BUTTON_PARAM, "Chaos CV: MONO", "Chaos CV: STEREO");
#endif

                updateToggleButtonTooltip(CHAOS_FREEZE_BUTTON_PARAM, "Chaos engine: RUNNING", "Chaos engine: STOPPED");
                updateToggleButtonTooltip(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM, "Display chaos voltages: NO", "Display chaos voltages: YES");
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


        struct SampleHoldButton : SapphireTinyToggleButton
        {
            explicit SampleHoldButton()
            {
                addTinyButtonFrames(this, "red");
            }
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
                addSampleHoldButton();
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
                addChaosBox();
                addOverlays();
            }

            void addSampleHoldButton()
            {
                auto button = createParamCentered<SampleHoldButton>(Vec{}, belleModule, SAMPLE_HOLD_BUTTON_PARAM);
                addSapphireParam(button, "sample_hold_button");
            }

            void addChaosBox()
            {
                addSnapVoctFlatControlGroup("cspeed", CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT);
                addSapphireFlatControlGroup("clevel", CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT);
                addChaosStereoButton();
                addChaosRandomButton();
                addChaosFreezeButton();
                addChaosDisplayVoltagesButton();
            }

            void addChaosStereoButton()
            {
#ifdef ENABLE_CHAOS_STEREO_BUTTON
                auto button = createParamCentered<ChaosStereoButton>(Vec{}, belleModule, CHAOS_STEREO_BUTTON_PARAM);
                addSapphireParam(button, "chaos_stereo_button");
#endif
            }

            void addChaosRandomButton()
            {
                auto button = createParamCentered<ChaosRandomButton>(Vec{}, belleModule, CHAOS_RANDOMIZE_BUTTON_PARAM);
                button->parentWidget = this;
                addSapphireParam(button, "chaos_random_button");
            }

            void addChaosFreezeButton()
            {
                auto button = createParamCentered<ChaosFreezeButton>(Vec{}, belleModule, CHAOS_FREEZE_BUTTON_PARAM);
                addSapphireParam(button, "chaos_freeze_button");
            }

            void addChaosDisplayVoltagesButton()
            {
                auto button = createParamCentered<ChaosDisplayVoltagesButton>(Vec{}, belleModule, CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM);
                addSapphireParam(button, "chaos_display_button");
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
