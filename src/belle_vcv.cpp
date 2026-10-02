// Sapphire Belle for VCV Rack, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include <cassert>
#include <cstring>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_voice.hpp"
#include "chaos_fountain.hpp"
#include "sapphire_smoother.hpp"

namespace Sapphire
{
    namespace Belle
    {
        struct BelleModule;

        constexpr int OctaveRange = 4;            // +/- octave range around default frequency

        constexpr unsigned nChaoticSignals = 14;    // how many attenuverters need separate signals
        constexpr unsigned nBatchSize = std::max<unsigned>(PORT_MAX_CHANNELS, nChaoticSignals);
        using fountain_t = ChaosFountain<nBatchSize>;
        using batch_t = ChaosBatch<nBatchSize>;

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
            PITCH_MODE_BUTTON_PARAM,
            CHAOS_RANDOMIZE_BUTTON_PARAM,
            CHAOS_FREEZE_BUTTON_PARAM,
            CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM,
            PAN_PARAM,
            PAN_ATTEN,
            OUTPUT_MODE_BUTTON_PARAM,
            GLIDE_PARAM,
            GLIDE_ATTEN,
            LEVEL_PARAM,
            LEVEL_ATTEN,
            FREQ_MODE_BUTTON_PARAM,
            OCT_MODE_BUTTON_PARAM,
            CHAOS_SPREAD_PARAM,
            CHAOS_SPREAD_ATTEN,

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
            PAN_CV_INPUT,
            GLIDE_CV_INPUT,
            LEVEL_CV_INPUT,
            CHAOS_SPREAD_CV_INPUT,

            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_LEFT_OUTPUT,
            AUDIO_RIGHT_OUTPUT,
            ENVELOPE_OUTPUT,

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
            Smoother chaosSeedSmoother{0.025};
            Smoother modelChangeSmoother{0.015};
            bool requestSeedSplash = false;
            uint64_t seedToRestore = 0;

            AdsrVoiceEngine<SineEngine> polySine{"sine", "Detune", "Distortion", "Sin2", "Sin3"};
            AdsrVoiceEngine<TriangleEngine> polyTriangle{"triangle", "Detune", "Tri1", "Tri2", "Tri3"};
            AdsrVoiceEngine<SawEngine> polySaw{"saw", "Detune", "Saw1", "Saw2", "Saw3"};
            AdsrVoiceEngine<SquareEngine> polySquare{"square", "Detune", "PWM", "Sqr2", "Sqr3"};
            std::vector<PolyStereoVoice*> polyEngineList;
            unsigned currentEngineIndex{};
            float mildSensitivityLevel = 0.2;

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

                configOutput(ENVELOPE_OUTPUT, "Envelope");
                configOutput(AUDIO_LEFT_OUTPUT,  "Left audio");
                configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");
                configButton(OUTPUT_MODE_BUTTON_PARAM);
                configPitchModeButton(PITCH_MODE_BUTTON_PARAM, "Pitch tracker");
                configPitchModeButton(FREQ_MODE_BUTTON_PARAM,  "Frequency tracker");
                configPitchModeButton(OCT_MODE_BUTTON_PARAM,   "Octave tracker");

                configControlGroup("Frequency", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT, -OctaveRange, +OctaveRange, 0);
                configControlGroup("Octave", OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT, -OctaveRange, +OctaveRange, 0);
                paramQuantities.at(OCT_PARAM)->snapEnabled = true;

                configControlGroup("Pitch glide (portamento)", GLIDE_PARAM, GLIDE_ATTEN, GLIDE_CV_INPUT, -2, +1, -1, " sec/oct", 10);
                configControlGroup("Panning", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT, -1, +1, 0, "%", 0, 100);
                configControlGroup("Attack", ATTACK_PARAM, ATTACK_ATTEN, ATTACK_CV_INPUT);
                configControlGroup("Decay", DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                configControlGroup("Sustain", SUSTAIN_PARAM, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT, 0, 1, 0.5, "%", 0, 100);
                configControlGroup("Release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);
                configControlGroup("Output level", LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);

                for (int m = 0; m < 4; ++m)
                    configControlGroup("", MOD_PARAM_0 + m, MOD_ATTEN_0 + m, MOD_CV_INPUT_0 + m);

                configParam(MODEL_SELECT_PARAM, 0, engineCount()-1, DefaultEngineIndex, "Model");
                paramQuantities.at(MODEL_SELECT_PARAM)->snapEnabled = true;

                attenuverterChaosOptIn(FREQ_ATTEN,      1);
                attenuverterChaosOptIn(OCT_ATTEN,       1);
                attenuverterChaosOptIn(GLIDE_ATTEN,     mildSensitivityLevel);
                attenuverterChaosOptIn(PAN_ATTEN,       mildSensitivityLevel);
                attenuverterChaosOptIn(ATTACK_ATTEN,    mildSensitivityLevel);
                attenuverterChaosOptIn(DECAY_ATTEN,     mildSensitivityLevel);
                attenuverterChaosOptIn(SUSTAIN_ATTEN,   mildSensitivityLevel);
                attenuverterChaosOptIn(RELEASE_ATTEN,   mildSensitivityLevel);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 0, mildSensitivityLevel);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 1, mildSensitivityLevel);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 2, mildSensitivityLevel);
                attenuverterChaosOptIn(MOD_ATTEN_0 + 3, mildSensitivityLevel);
                attenuverterChaosOptIn(LEVEL_ATTEN,     1);

                configChaosBox();

                initialize();
            }

            void configPitchModeButton(int buttonParamId, const std::string& name)
            {
                configSwitch(buttonParamId, 0, 2, 0, name, {"FOLLOW", "SAMPLE & HOLD", "GLIDE"});
            }

            void configChaosBox()
            {
                configControlGroup("Chaos speed",  CHAOS_SPEED_PARAM, CHAOS_SPEED_ATTEN, CHAOS_SPEED_CV_INPUT, -ChaosOctaveRange, +ChaosOctaveRange);
                configControlGroup("Chaos level",  CHAOS_LEVEL_PARAM, CHAOS_LEVEL_ATTEN, CHAOS_LEVEL_CV_INPUT, 0, 2, 1, " dB", -10, 20*3);
                configControlGroup("Chaos spread", CHAOS_SPREAD_PARAM, CHAOS_SPREAD_ATTEN, CHAOS_SPREAD_CV_INPUT, -1, +1, 0, "%", 0, 100);
                configButton(CHAOS_RANDOMIZE_BUTTON_PARAM, "Randomize chaotic CV");
                configButton(CHAOS_FREEZE_BUTTON_PARAM);
                configButton(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM);
                attenuverterChaosOptIn(CHAOS_SPEED_ATTEN);
            }

            bool shouldDisplayChaosVoltages() override
            {
                return isButtonEnabled(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM);
            }

            unsigned chaosVoltagesCount() override
            {
                return 16;
            }

            unsigned engineCount() const
            {
                return polyEngineList.size();
            }

            void initialize()
            {
                fountain.reset();
                speedChaos = 0;
                chaosSeedSmoother.initialize();
                modelChangeSmoother.initialize();
                params.at(OUTPUT_MODE_BUTTON_PARAM).setValue(1);    // polyphonic output by default
                params.at(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM).setValue(1);     // display voltage colors on attenuverters by default
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

            void updateSelectedEngine(float sampleRateHz)
            {
                const unsigned targetEngineIndex = static_cast<unsigned>(params.at(MODEL_SELECT_PARAM).getValue());
                const bool changing = (targetEngineIndex != currentEngineIndex);

                if (modelChangeSmoother.isStable() && changing)
                    modelChangeSmoother.begin();

                modelChangeSmoother.process(sampleRateHz);
                if (modelChangeSmoother.isDelayedActionReady() && changing)
                {
                    // Silence the current engine before leaving.
                    // Otherwise it leaves residual energy in the system when we come back.
                    getCurrentEngine().initialize();

                    // Switch to the new engine.
                    currentEngineIndex = targetEngineIndex;
                }
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

                const float chaosLevelKnob = Cube(getControlValueVoltPerOctave(
                    CHAOS_LEVEL_PARAM,
                    CHAOS_LEVEL_ATTEN,
                    CHAOS_LEVEL_CV_INPUT,
                    0,
                    2
                ));

                const float spread = getControlValueVoltPerOctave(
                    CHAOS_SPREAD_PARAM,
                    CHAOS_SPREAD_ATTEN,
                    CHAOS_SPREAD_CV_INPUT,
                    -1,
                    +1,
                    0.2
                );

                if (!isChaosFrozen)
                {
                    const float dt = SimulationTimeIncrement(sampleRateHz, speedKnob);
                    fountain.update(dt);
                }

                batch = fountain.getBatch(chaosLevelKnob);

                reportChaosPoly(FREQ_ATTEN,         spread, batch,  0);
                reportChaosPoly(OCT_ATTEN,          spread, batch,  1);
                reportChaosPoly(ATTACK_ATTEN,       spread, batch,  2);
                reportChaosPoly(DECAY_ATTEN,        spread, batch,  3);
                reportChaosPoly(SUSTAIN_ATTEN,      spread, batch,  4);
                reportChaosPoly(RELEASE_ATTEN,      spread, batch,  5);
                reportChaosPoly(MOD_ATTEN_0+0,      spread, batch,  6);
                reportChaosPoly(MOD_ATTEN_0+1,      spread, batch,  7);
                reportChaosPoly(MOD_ATTEN_0+2,      spread, batch,  8);
                reportChaosPoly(MOD_ATTEN_0+3,      spread, batch,  9);
                reportChaosPoly(CHAOS_SPEED_ATTEN,  spread, batch, 10);
                reportChaosPoly(PAN_ATTEN,          spread, batch, 11);
                reportChaosPoly(LEVEL_ATTEN,        spread, batch, 12);
                reportChaosPoly(GLIDE_ATTEN,        spread, batch, 13);

                chaosSeedSmoother.process(sampleRateHz);
                if (chaosSeedSmoother.isDelayedActionReady() && seedToRestore)
                {
                    fountain.reset(seedToRestore);
                    seedToRestore = 0;
                }
            }

            float chaosSignal(const batch_t& batch, int attenId, unsigned channel) const
            {
                const SapphireAttenuverterContext& context = paramInfo.at(attenId).context;
                return batch(context.chaosOffset + channel);
            }

            void reportChaosPoly(int attenId, float spread, const batch_t& batch, unsigned offset)
            {
                SapphireAttenuverterContext& context = paramInfo.at(attenId).context;
                context.chaosOffset = offset;
                const float vneg = batch(offset+0);
                const float vpos = batch(offset+1);
                for (unsigned c = 0; c < CHAOS_MAX_CHANNELS; ++c)
                {
                    const float vc = batch(offset+c);
                    if (spread < 0)
                        context.chaosVoltage[c] = LinearMix(-spread, vc, vneg);
                    else
                        context.chaosVoltage[c] = LinearMix(+spread, vc, vpos);
                }
            }

            PitchMode getPitchMode(int buttonParamId)
            {
                float v = params.at(buttonParamId).getValue();
                int n = static_cast<int>(std::round(v));
                return static_cast<PitchMode>(n);
            }

            float nextSignal(float& voltage, int attenId, int inputId, unsigned channel, batch_t& batch)
            {
                Input& input = inputs.at(inputId);
                if (!input.isConnected())
                    voltage = chaosSignal(batch, attenId, channel);
                else if (channel < static_cast<unsigned>(input.getChannels()))
                    voltage = input.getVoltage(channel);
                return voltage;
            }

            void process(const ProcessArgs& args) override
            {
                updateSelectedEngine(args.sampleRate);

                auto& left  = outputs.at(AUDIO_LEFT_OUTPUT);
                auto& right = outputs.at(AUDIO_RIGHT_OUTPUT);
                auto& env   = outputs.at(ENVELOPE_OUTPUT);

                if (unsigned nPolyChannels = numOutputChannels(INPUTS_LEN, 0); nPolyChannels > 0)
                {
                    PolyStereoVoice& polyEngine = getCurrentEngine();

                    batch_t batch;
                    updateChaos(args.sampleRate, batch);
                    speedChaos = batch(10);

                    const PitchMode pitchMode = getPitchMode(PITCH_MODE_BUTTON_PARAM);
                    const PitchMode freqMode  = getPitchMode(FREQ_MODE_BUTTON_PARAM);
                    const PitchMode octMode   = getPitchMode(OCT_MODE_BUTTON_PARAM);

                    float levelVoltage = 0;
                    float gateVoltage = 0;
                    float pitchVoltage = 0;
                    float freqVoltage = 0;
                    float octaveVoltage = 0;
                    float glideVoltage = 0;
                    float panVoltage = 0;
                    float attackVoltage = 0;
                    float decayVoltage = 0;
                    float sustainVoltage = 0;
                    float releaseVoltage = 0;
                    float modVoltage[NUM_DYNAMIC_PARAMS]{};
                    float gain[PORT_MAX_CHANNELS]{};
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                    {
                        VoiceContext& context = polyEngine.contextArray[c];
                        context.trackerPitch.mode = pitchMode;
                        context.trackerFreq.mode = freqMode;
                        context.trackerOct.mode = octMode;

                        nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                        nextChannelInputVoltage(pitchVoltage, PITCH_INPUT, c);
                        nextSignal(freqVoltage, FREQ_ATTEN, FREQ_CV_INPUT, c, batch);
                        nextSignal(octaveVoltage, OCT_ATTEN, OCT_CV_INPUT, c, batch);
                        nextSignal(attackVoltage, ATTACK_ATTEN, ATTACK_CV_INPUT, c, batch);
                        nextSignal(decayVoltage, DECAY_ATTEN, DECAY_CV_INPUT, c, batch);
                        nextSignal(sustainVoltage, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT, c, batch);
                        nextSignal(releaseVoltage, RELEASE_ATTEN, RELEASE_CV_INPUT, c, batch);
                        for (unsigned m = 0; m < NUM_DYNAMIC_PARAMS; ++m)
                            nextSignal(modVoltage[m], MOD_ATTEN_0+m, MOD_CV_INPUT_0+m, c, batch);
                        nextSignal(panVoltage, PAN_ATTEN, PAN_CV_INPUT, c, batch);
                        nextSignal(levelVoltage, LEVEL_ATTEN, LEVEL_CV_INPUT, c, batch);
                        nextSignal(glideVoltage, GLIDE_ATTEN, GLIDE_CV_INPUT, c, batch);

                        static constexpr float gainSensitivity = 1.0 / 5.0;    // one knob unit per 5V change in CV
                        gain[c] = Cube(cvGetVoltPerOctave(LEVEL_PARAM, LEVEL_ATTEN, levelVoltage * gainSensitivity, 0, 2));
                        float freq = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, freqVoltage, -OctaveRange, +OctaveRange);
                        float oct = std::round(cvGetVoltPerOctave(OCT_PARAM, OCT_ATTEN, octaveVoltage, -OctaveRange, +OctaveRange));
                        context.pan = cvGetVoltPerOctave(PAN_PARAM, PAN_ATTEN, panVoltage, -1, +1);
                        const float glideKnob = cvGetVoltPerOctave(GLIDE_PARAM, GLIDE_ATTEN, glideVoltage, -2, +1);
                        const float glideSamples = TenToPower<float>(glideKnob) * args.sampleRate;
                        context.updateGatePitch(gateVoltage, pitchVoltage, freq, oct, 1/glideSamples);
                        context.attack  = cvGetVoltPerOctave(ATTACK_PARAM,  ATTACK_ATTEN,  attackVoltage,  -1, +1);
                        context.decay   = cvGetVoltPerOctave(DECAY_PARAM,   DECAY_ATTEN,   decayVoltage,   -1, +1);
                        context.sustain = cvGetVoltPerOctave(SUSTAIN_PARAM, SUSTAIN_ATTEN, sustainVoltage,  0, +1);
                        context.release = cvGetVoltPerOctave(RELEASE_PARAM, RELEASE_ATTEN, releaseVoltage, -1, +1);
                        for (unsigned m = 0; m < NUM_DYNAMIC_PARAMS; ++m)
                            context.mod[m] = cvGetVoltPerOctave(MOD_PARAM_0+m, MOD_ATTEN_0+m, modVoltage[m], -1, +1);
                    }

                    PolyResult result = polyEngine.process(args.sampleRate, nPolyChannels);

                    const float antiClick =
                        chaosSeedSmoother.getGain() *
                        modelChangeSmoother.getGain();

                    env.setChannels(nPolyChannels);
                    for (unsigned c = 0; c < nPolyChannels; ++c)
                        env.setVoltage(10 * result.env.poly[c] * antiClick, c);

                    if (isOutputModePolyphonic())
                    {
                        left.setChannels(nPolyChannels);
                        right.setChannels(nPolyChannels);
                        for (unsigned c = 0; c < nPolyChannels; ++c)
                        {
                            left .setVoltage(result.stereo.poly[c].sample[0] * antiClick * gain[c], c);
                            right.setVoltage(result.stereo.poly[c].sample[1] * antiClick * gain[c], c);
                        }
                    }
                    else
                    {
                        left.setChannels(1);
                        right.setChannels(1);
                        float sumL = 0;
                        float sumR = 0;
                        for (unsigned c = 0; c < nPolyChannels; ++c)
                        {
                            sumL += result.stereo.poly[c].sample[0] * gain[c];
                            sumR += result.stereo.poly[c].sample[1] * gain[c];
                        }
                        left .setVoltage(sumL * antiClick, 0);
                        right.setVoltage(sumR * antiClick, 0);
                    }
                }
                else
                {
                    left.setChannels(1);
                    left.setVoltage(0, 0);

                    right.setChannels(1);
                    right.setVoltage(0, 0);

                    env.setChannels(1);
                    env.setVoltage(0, 0);
                }
            }

            bool isOutputModePolyphonic()
            {
                return isButtonEnabled(OUTPUT_MODE_BUTTON_PARAM);
            }

            void updateControls()
            {
                const PolyStereoVoice& engine = getCurrentEngine();
                updateDynamicControlGroup(MOD_PARAM_0+0, MOD_ATTEN_0+0, MOD_CV_INPUT_0+0, engine.mod0);
                updateDynamicControlGroup(MOD_PARAM_0+1, MOD_ATTEN_0+1, MOD_CV_INPUT_0+1, engine.mod1);
                updateDynamicControlGroup(MOD_PARAM_0+2, MOD_ATTEN_0+2, MOD_CV_INPUT_0+2, engine.mod2);
                updateDynamicControlGroup(MOD_PARAM_0+3, MOD_ATTEN_0+3, MOD_CV_INPUT_0+3, engine.mod3);

                updateToggleButtonTooltip(CHAOS_FREEZE_BUTTON_PARAM, "Chaos engine: RUNNING", "Chaos engine: STOPPED");
                updateToggleButtonTooltip(CHAOS_DISPLAY_VOLTAGES_BUTTON_PARAM, "Display chaos voltages: NO", "Display chaos voltages: YES");

                updateParamTooltip(
                    CHAOS_RANDOMIZE_BUTTON_PARAM,
                    "Randomize chaotic CV\nseed = " + SeedString(fountain.getSeed())
                );

                updateToggleButtonTooltip(
                    OUTPUT_MODE_BUTTON_PARAM,
                    "Output format: (L monophonic, R monophonic)",
                    "Output format: (L polyphonic, R polyphonic)"
                );
            }

            void updateDynamicControlGroup(int paramId, int attenId, int inputId, const std::string& name)
            {
                getParamQuantity(paramId)->name = name;
                getParamQuantity(attenId)->name = name + " attenuverter";
                getInputInfo(inputId)->name = name + " CV";
            }

            void beginSeedChangeAntiClick(uint64_t seed) override
            {
                seedToRestore = seed;
                requestSeedSplash = true;
                chaosSeedSmoother.begin();
            }

            void randomizeChaos()
            {
                InvokeAction(new RandomizeChaosAction({
                    ChaosFountainRestoreInfo(id, fountain.getSeed())
                }));
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


        struct PitchModeButton : SapphireTinyToggleButton
        {
            explicit PitchModeButton()
            {
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/yellow_button_0.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/red_button_1.svg")));
                addFrame(Svg::load(asset::plugin(pluginInstance, "res/green_button_1.svg")));
            }
        };


        struct OutputModeButton : SapphireTinyToggleButton
        {
            explicit OutputModeButton()
            {
                addTinyButtonFrames(this, "green");
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
                addSapphireOutput(ENVELOPE_OUTPUT, "envelope_output");
                addOutputModeButton();
                addPitchModeButton(PITCH_MODE_BUTTON_PARAM, "pitch_mode_button");
                addPitchModeButton(FREQ_MODE_BUTTON_PARAM, "freq_mode_button");
                addPitchModeButton(OCT_MODE_BUTTON_PARAM, "oct_mode_button");
                addSnapVoctFlatControlGroup("freq", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
                addSnapVoctFlatControlGroup("oct", OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT);
                addSapphireFlatControlGroup("glide", GLIDE_PARAM, GLIDE_ATTEN, GLIDE_CV_INPUT);
                addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                addSapphireFlatControlGroup("attack", ATTACK_PARAM, ATTACK_ATTEN, ATTACK_CV_INPUT);
                addSapphireFlatControlGroup("decay", DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                addSapphireFlatControlGroup("sustain", SUSTAIN_PARAM, SUSTAIN_ATTEN, SUSTAIN_CV_INPUT);
                addSapphireFlatControlGroup("release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);
                addSapphireControlGroup("level", LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT);

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

            void addOutputModeButton()
            {
                auto button = createParamCentered<OutputModeButton>(Vec{}, belleModule, OUTPUT_MODE_BUTTON_PARAM);
                addSapphireParam(button, "output_mode_button");
            }

            void randomizeChaos() override
            {
                if (belleModule)
                    belleModule->randomizeChaos();
            }

            void addPitchModeButton(int buttonParamId, const std::string& label)
            {
                auto button = createParamCentered<PitchModeButton>(Vec{}, belleModule, buttonParamId);
                addSapphireParam(button, label);
            }

            void addChaosBox()
            {
                addSnapVoctFlatControlGroup("cspeed",  CHAOS_SPEED_PARAM,  CHAOS_SPEED_ATTEN,  CHAOS_SPEED_CV_INPUT );
                addSapphireFlatControlGroup("clevel",  CHAOS_LEVEL_PARAM,  CHAOS_LEVEL_ATTEN,  CHAOS_LEVEL_CV_INPUT );
                addSapphireFlatControlGroup("cspread", CHAOS_SPREAD_PARAM, CHAOS_SPREAD_ATTEN, CHAOS_SPREAD_CV_INPUT);
                addChaosRandomButton();
                addChaosFreezeButton();
                addChaosDisplayVoltagesButton();
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
                    if (belleModule->requestSeedSplash)
                    {
                        belleModule->requestSeedSplash = false;
                        splash.begin(0x80, 0x40, 0x80, 0.1, 0.25);
                    }
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
