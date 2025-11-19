#include <array>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "vina_engine.hpp"

namespace Sapphire      // Indranīla (इन्द्रनील)
{
    namespace Vina      // (Sanskrit: वीणा IAST: vīṇā)
    {
        constexpr int MinOctave = -2;
        constexpr int MaxOctave = +2;
        constexpr float CenterFreqHz = 261.6255653005986;   // C4 = 440/(2**0.75) Hz

        enum ParamId
        {
            DYNAMIC_WIRE_BUTTON_PARAM,
            FREQ_PARAM,
            FREQ_ATTEN,
            OCT_PARAM,
            OCT_ATTEN,
            LEVEL_PARAM,
            LEVEL_ATTEN,
            DECAY_PARAM,
            DECAY_ATTEN,
            RELEASE_PARAM,
            RELEASE_ATTEN,
            PAN_PARAM,
            PAN_ATTEN,
            SPREAD_PARAM,
            SPREAD_ATTEN,
            TONE_PARAM,
            TONE_ATTEN,
            STEREO_BUTTON_PARAM,
            _OBSOLETE_3_PARAM,
            CHORUS_DEPTH_PARAM,
            CHORUS_DEPTH_ATTEN,
            CHORUS_RATE_PARAM,
            CHORUS_RATE_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            GATE_INPUT,
            VOCT_INPUT,
            FREQ_CV_INPUT,
            OCT_CV_INPUT,
            LEVEL_CV_INPUT,
            DECAY_CV_INPUT,
            RELEASE_CV_INPUT,
            PAN_CV_INPUT,
            SPREAD_CV_INPUT,
            TONE_CV_INPUT,
            _OBSOLETE_1_CV_INPUT,
            CHORUS_DEPTH_CV_INPUT,
            CHORUS_RATE_CV_INPUT,
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


        struct ChannelInfo
        {
            GateTriggerReceiver gateReceiver;
            int activeWireIndex = -1;

            void initialize()
            {
                gateReceiver.initialize();
                activeWireIndex = -1;
            }
        };


        struct VinaModule : SapphireModule
        {
            int numActiveChannels = 0;
            std::array<ChannelInfo, PORT_MAX_CHANNELS> channelInfo;
            std::array<VinaWire, PORT_MAX_CHANNELS> wire;
            PortLabelMode outputPortMode = PortLabelMode::Stereo;
            long pluckCounter = 0;

            explicit VinaModule()
                : SapphireModule(PARAMS_LEN, OUTPUTS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                if (auto dwButton = configButton(DYNAMIC_WIRE_BUTTON_PARAM))
                {
                    dwButton->defaultValue = 1;
                    dwButton->setValue(1);
                }

                if (auto stereoButton = configButton(STEREO_BUTTON_PARAM))
                {
                    stereoButton->defaultValue = 1;
                    stereoButton->setValue(1);
                }

                configParam(FREQ_PARAM, MinOctave, MaxOctave, 0, "Frequency", " Hz", 2, CenterFreqHz);
                configAtten(FREQ_ATTEN, "Frequency");
                configInput(FREQ_CV_INPUT, "Frequency CV");

                if (ParamQuantity* octParam = configParam(OCT_PARAM, MinOctave, MaxOctave, 0, "Octave"))
                    octParam->snapEnabled = true;
                configAtten(OCT_ATTEN, "Octave");
                configInput(OCT_CV_INPUT, "Octave CV");

                configParam(LEVEL_PARAM, 0, 2, 1, "Level", " dB", -10, 20*3);
                configAtten(LEVEL_ATTEN, "Level CV");
                configInput(LEVEL_CV_INPUT, "Level CV");

                configParam(PAN_PARAM, -1, +1, 0, "Pan");
                configAtten(PAN_ATTEN, "Pan CV");
                configInput(PAN_CV_INPUT, "Pan CV");

                configParam(SPREAD_PARAM, 0, 1, 0, "Spread");
                configAtten(SPREAD_ATTEN, "Spread CV");
                configInput(SPREAD_CV_INPUT, "Spread CV");

                configParam(DECAY_PARAM, 0, 1, 0.5, "Decay");
                configAtten(DECAY_ATTEN, "Decay CV");
                configInput(DECAY_CV_INPUT, "Decay CV");

                configParam(RELEASE_PARAM, 0, 1, 0.5, "Release");
                configAtten(RELEASE_ATTEN, "Release CV");
                configInput(RELEASE_CV_INPUT, "Release CV");

                configParam(TONE_PARAM, -1, +1, 0, "Tone");
                configAtten(TONE_ATTEN, "Tone CV");
                configInput(TONE_CV_INPUT, "Tone CV");

                configParam(CHORUS_DEPTH_PARAM, 0, 1, 0.5, "Chorus depth");
                configAtten(CHORUS_DEPTH_ATTEN, "Chorus depth CV");
                configInput(CHORUS_DEPTH_CV_INPUT, "Chorus depth CV");

                configParam(CHORUS_RATE_PARAM, -1, +1, 0, "Chorus rate");
                configAtten(CHORUS_RATE_ATTEN, "Chorus rate CV");
                configInput(CHORUS_RATE_CV_INPUT, "Chorus rate CV");

                configInput(GATE_INPUT, "Gate");
                configInput(VOCT_INPUT, "V/OCT");
                configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
                configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

                initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void initialize()
            {
                for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                {
                    channelInfo[c].initialize();
                    wire[c].initialize();

                    // Start out with wire[c] assigned to channelInfo[c].
                    // That way we start out without having to search for a wire.
                    channelInfo[c].activeWireIndex = c;
                    wire[c].assignedPolyChannel = c;
                }
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
            }

            bool isDynamicWireAssignmentEnabled()
            {
                return getParamQuantity(DYNAMIC_WIRE_BUTTON_PARAM)->getValue() > 0.5f;
            }

            bool isPolyphonicStereoEnabled()
            {
                return getParamQuantity(STEREO_BUTTON_PARAM)->getValue() > 0.5f;
            }

            bool needToPickNewWire(int c, bool trigger)
            {
                // Do we have a currently assigned wire?
                const int wi = channelInfo[c].activeWireIndex;
                if (!IsValidIndex(wi))
                    return true;

                // When triggered, we don't want to stop the current wire's vibration.
                // Is the current wire still vibrating?
                // If it is quiet, we can keep using it.
                // Otherwise, we try to pick a different wire.
                return trigger && isDynamicWireAssignmentEnabled() && !wire[wi].isQuiet();
            }

            void makeChannelOwnWire(int c, int k)
            {
                assert(IsValidIndex(c));
                assert(IsValidIndex(k));

                for (int i = 0; i < PORT_MAX_CHANNELS; ++i)
                    if (i != c && channelInfo[i].activeWireIndex == k)
                        channelInfo[i].activeWireIndex = InvalidIndex;

                channelInfo[c].activeWireIndex = k;
                wire[k].assignedPolyChannel = c;
            }

            void claimQuietestWire(int c)
            {
                // Steal any quiet wire we can find, from any channel.
                for (int k = 0; k < PORT_MAX_CHANNELS; ++k)
                    if (wire[k].isQuiet())
                        return makeChannelOwnWire(c, k);

                // Fallback: pick the wire we plucked the longest in the past.
                int wi = 0;
                for (int k = 1; k < PORT_MAX_CHANNELS; ++k)
                    if (wire[k].pluckCounter < wire[wi].pluckCounter)
                        wi = k;

                makeChannelOwnWire(c, wi);
            }

            VinaWire& pickWire(int c, bool trigger)
            {
                if (needToPickNewWire(c, trigger))
                    claimQuietestWire(c);

                return wire[channelInfo[c].activeWireIndex];
            }

            VinaStereoFrame updateWiresForChannel(int c, float sampleRateHz, bool gate, bool trigger)
            {
                VinaStereoFrame frame;
                for (int k = 0; k < PORT_MAX_CHANNELS; ++k)
                {
                    if (wire[k].assignedPolyChannel == c)
                    {
                        const bool isActive = (channelInfo[c].activeWireIndex == k);
                        frame += wire[k].update(sampleRateHz, isActive && gate, isActive && trigger);
                    }
                }
                return frame;
            }

            void process(const ProcessArgs& args) override
            {
                numActiveChannels = numOutputChannels(INPUTS_LEN, 1);
                const bool polyphonicOutput = isPolyphonicStereoEnabled();
                const int numOutputChannels = polyphonicOutput ? numActiveChannels : 1;
                outputs[AUDIO_LEFT_OUTPUT ].setChannels(numOutputChannels);
                outputs[AUDIO_RIGHT_OUTPUT].setChannels(numOutputChannels);
                float gateVoltage = 0;
                float voctVoltage = 0;
                float cvFreq = 0;
                float cvOct = 0;
                float cvLevel = 0;
                float cvDecay = 0;
                float cvRelease = 0;
                float cvPan = 0;
                float cvSpread = 0;
                float cvTone = 0;
                float cvChorusDepth = 0;
                float cvChorusRate = 0;

                VinaStereoFrame sum;

                for (int c = 0; c < numActiveChannels; ++c)
                {
                    ChannelInfo& q = channelInfo[c];

                    nextChannelInputVoltage(gateVoltage, GATE_INPUT, c);
                    nextChannelInputVoltage(voctVoltage, VOCT_INPUT, c);

                    nextChannelInputVoltage(cvFreq, FREQ_CV_INPUT, c);
                    nextChannelInputVoltage(cvOct, OCT_CV_INPUT, c);
                    nextChannelInputVoltage(cvLevel, LEVEL_CV_INPUT, c);
                    nextChannelInputVoltage(cvDecay, DECAY_CV_INPUT, c);
                    nextChannelInputVoltage(cvRelease, RELEASE_CV_INPUT, c);
                    nextChannelInputVoltage(cvPan, PAN_CV_INPUT, c);
                    nextChannelInputVoltage(cvSpread, SPREAD_CV_INPUT, c);
                    nextChannelInputVoltage(cvTone, TONE_CV_INPUT, c);
                    nextChannelInputVoltage(cvChorusDepth, CHORUS_DEPTH_CV_INPUT, c);
                    nextChannelInputVoltage(cvChorusRate, CHORUS_RATE_CV_INPUT, c);

                    q.gateReceiver.update(gateVoltage);
                    bool gate = q.gateReceiver.isGateActive();
                    bool trigger = q.gateReceiver.isTriggerActive();
                    float rawFreq = cvGetVoltPerOctave(FREQ_PARAM, FREQ_ATTEN, cvFreq, MinOctave, MaxOctave);
                    float oct = cvGetVoltPerOctave(OCT_PARAM,  OCT_ATTEN,  cvOct,  MinOctave, MaxOctave);
                    float level = cvGetControlValue(LEVEL_PARAM, LEVEL_ATTEN, cvLevel, 0, 2);
                    float pan = cvGetControlValue(PAN_PARAM, PAN_ATTEN, cvPan, -1, +1);
                    float spread = cvGetControlValue(SPREAD_PARAM, SPREAD_ATTEN, cvSpread, 0, +1);
                    float decay = cvGetControlValue(DECAY_PARAM, DECAY_ATTEN, cvDecay, 0, 1);
                    float release = cvGetControlValue(RELEASE_PARAM, RELEASE_ATTEN, cvRelease, 0, 1);
                    float tone = cvGetControlValue(TONE_PARAM, TONE_ATTEN, cvTone, -1, +1);
                    float chorusDepth = cvGetControlValue(CHORUS_DEPTH_PARAM, CHORUS_DEPTH_ATTEN, cvChorusDepth, 0, 1);
                    float chorusRate = cvGetControlValue(CHORUS_RATE_PARAM, CHORUS_RATE_ATTEN, cvChorusRate, -1, +1);

                    bool validPitch = true;
                    float freq = rawFreq + std::round(oct) + voctVoltage;
                    if (freq < MinOctave-1 || freq > MaxOctave+1)
                        validPitch = trigger = gate = false;       // ignore notes outside the instrument's range

                    VinaWire& w = pickWire(c, trigger);

                    if (trigger)
                        w.pluckCounter = ++pluckCounter;

                    w.setLevel(level);
                    w.setDecay(decay);
                    w.setRelease(release);
                    w.setPan(pan);
                    w.setSpread(spread);
                    w.setChorusDepth(chorusDepth);
                    w.setChorusRate(chorusRate);
                    w.setTone(tone);

                    if (validPitch)
                        w.setPitch(freq);     // must set pitch AFTER tone

                    VinaStereoFrame frame = updateWiresForChannel(c, args.sampleRate, gate, trigger);
                    if (polyphonicOutput)
                    {
                        outputs[AUDIO_LEFT_OUTPUT ].setVoltage(frame.sample[0], c);
                        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(frame.sample[1], c);
                    }
                    else
                    {
                        sum += frame;
                    }
                }

                if (!polyphonicOutput)
                {
                    outputs[AUDIO_LEFT_OUTPUT ].setVoltage(sum.sample[0], 0);
                    outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sum.sample[1], 0);
                }
            }
        };


        struct DynamicWireButton : SapphireTinyToggleButton
        {
            explicit DynamicWireButton()
            {
                addTinyButtonFrames(this, "green");
            }
        };


        struct StereoButton : SapphireTinyToggleButton
        {
            explicit StereoButton()
            {
                addTinyButtonFrames(this, "yellow");
            }
        };


        struct VinaWidget : SapphireWidget
        {
            VinaModule* vinaModule{};

            explicit VinaWidget(VinaModule *module)
                : SapphireWidget("vina", asset::plugin(pluginInstance, "res/vina.svg"))
                , vinaModule(module)
            {
                setModule(module);
                addDynamicWireButton();
                addStereoButton();
                addSapphireInput(GATE_INPUT, "gate_input");
                addSapphireInput(VOCT_INPUT, "voct_input");
                addSapphireOutput(AUDIO_LEFT_OUTPUT,  "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");
                addSapphireFlatControlGroup("freq", FREQ_PARAM, FREQ_ATTEN, FREQ_CV_INPUT);
                addSapphireFlatControlGroup("oct", OCT_PARAM, OCT_ATTEN, OCT_CV_INPUT);
                addSapphireFlatControlGroup("level", LEVEL_PARAM, LEVEL_ATTEN, LEVEL_CV_INPUT);
                addSapphireFlatControlGroup("spread", SPREAD_PARAM, SPREAD_ATTEN, SPREAD_CV_INPUT);
                addSapphireFlatControlGroup("pan", PAN_PARAM, PAN_ATTEN, PAN_CV_INPUT);
                addSapphireFlatControlGroup("decay", DECAY_PARAM, DECAY_ATTEN, DECAY_CV_INPUT);
                addSapphireFlatControlGroup("release", RELEASE_PARAM, RELEASE_ATTEN, RELEASE_CV_INPUT);
                addSapphireFlatControlGroup("tone", TONE_PARAM, TONE_ATTEN, TONE_CV_INPUT);
                addSapphireFlatControlGroup("chorus_depth", CHORUS_DEPTH_PARAM, CHORUS_DEPTH_ATTEN, CHORUS_DEPTH_CV_INPUT);
                addSapphireFlatControlGroup("chorus_rate", CHORUS_RATE_PARAM, CHORUS_RATE_ATTEN, CHORUS_RATE_CV_INPUT);
            }

            void addDynamicWireButton()
            {
                auto button = createParamCentered<DynamicWireButton>(Vec{}, vinaModule, DYNAMIC_WIRE_BUTTON_PARAM);
                addSapphireParam(button, "dynamic_wire_button");
            }

            void addStereoButton()
            {
                auto button = createParamCentered<StereoButton>(Vec{}, vinaModule, STEREO_BUTTON_PARAM);
                addSapphireParam(button, "stereo_button");
            }

            void appendContextMenu(Menu* menu) override
            {
                SapphireWidget::appendContextMenu(menu);
            }

            void draw(const DrawArgs& args) override
            {
                SapphireWidget::draw(args);
                PortLabelMode outputPortMode = vinaModule ? vinaModule->outputPortMode : PortLabelMode::Stereo;
                drawAudioPortLabels(args.vg, outputPortMode, "left_output_label", "right_output_label");
            }

            void step() override
            {
                SapphireWidget::step();
                if (vinaModule)
                {
                    vinaModule->updateToggleButtonTooltip(
                        DYNAMIC_WIRE_BUTTON_PARAM,
                        "Single decaying note per channel (less CPU)",
                        "Multiple decaying notes per channel (more CPU)"
                    );

                    vinaModule->updateToggleButtonTooltip(
                        STEREO_BUTTON_PARAM,
                        "Monophonic output on L and R ports (unity mix)",
                        "Polyphonic output on L and R ports"
                    );
                }
            }
        };
    }
}


Model* modelSapphireVina = createSapphireModel<Sapphire::Vina::VinaModule, Sapphire::Vina::VinaWidget>(
    "Vina",
    Sapphire::ExpanderRole::None
);
