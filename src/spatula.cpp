#include "plugin.hpp"
#include "sapphire_spatula_engine.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    namespace Spatula
    {
        const int BlockExponent = 14;       // 2^BlockExponent samples per block
        const int BandCount = 5;            // the number of columns that each represent a frequency band

        enum ParamId
        {
            ENUMS(LEVEL_KNOB_PARAM, BandCount),
            ENUMS(LEVEL_ATTEN_PARAM, BandCount),
            ENUMS(DISPERSION_KNOB_PARAM, BandCount),
            ENUMS(DISPERSION_ATTEN_PARAM, BandCount),
            ENUMS(BANDWIDTH_KNOB_PARAM, BandCount),
            ENUMS(BANDWIDTH_ATTEN_PARAM, BandCount),
            ENUMS(CENTER_KNOB_PARAM, BandCount),
            ENUMS(CENTER_ATTEN_PARAM, BandCount),
            ENUMS(DELAY_KNOB_PARAM, BandCount),
            ENUMS(DELAY_ATTEN_PARAM, BandCount),
            ENUMS(FEEDBACK_KNOB_PARAM, BandCount),
            ENUMS(FEEDBACK_ATTEN_PARAM, BandCount),
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            ENUMS(LEVEL_CV_INPUT, BandCount),
            ENUMS(DISPERSION_CV_INPUT, BandCount),
            ENUMS(BANDWIDTH_CV_INPUT, BandCount),
            ENUMS(CENTER_CV_INPUT, BandCount),
            ENUMS(DELAY_CV_INPUT, BandCount),
            ENUMS(FEEDBACK_CV_INPUT, BandCount),
            INPUTS_LEN
        };

        enum OutputId
        {
            AUDIO_OUTPUT,
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };


        struct SpatulaModule : SapphireModule
        {
            FrameProcessor engine{BlockExponent};

            SpatulaModule()
                : SapphireModule(PARAMS_LEN)
            {
                const float OUTPUT_EXPONENT = 10;
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                for (int b = 0; b < BandCount; ++b)
                {
                    configParam(CENTER_KNOB_PARAM + b, -OctaveHalfRange, +OctaveHalfRange, 0, "Center frequency offset", " octaves");
                    configParam(CENTER_ATTEN_PARAM + b, -1, 1, 0, "Center frequency attenuverter", "%", 0, 100);
                    configInput(CENTER_CV_INPUT + b, "Center frequency CV");

                    configParam(LEVEL_KNOB_PARAM + b, 0, 2, 1, "Band level", " dB", -10, 20*OUTPUT_EXPONENT);
                    configParam(LEVEL_ATTEN_PARAM + b, -1, 1, 0, "Band level attenuverter", "%", 0, 100);
                    configInput(LEVEL_CV_INPUT + b, "Band level CV");

                    configParam(DISPERSION_KNOB_PARAM + b, 0, 180, 0, "Dispersion", "°");
                    configParam(DISPERSION_ATTEN_PARAM + b, -1, 1, 0, "Dispersion attenuverter", "%", 0, 100);
                    configInput(DISPERSION_CV_INPUT + b, "Dispersion CV");

                    configParam(BANDWIDTH_KNOB_PARAM + b, MinBandwidth, MaxBandwidth, 0, "Bandwidth", "");
                    configParam(BANDWIDTH_ATTEN_PARAM + b, -1, 1, 0, "Bandwidth attenuverter", "%", 0, 100);
                    configInput(BANDWIDTH_CV_INPUT + b, "Bandwidth CV");

                    configParam(DELAY_KNOB_PARAM + b, 0, MaxDelayMillis, 0, "Delay", " ms");
                    configParam(DELAY_ATTEN_PARAM + b, -1, 1, 0, "Delay attenuverter", "%", 0, 100);
                    configInput(DELAY_CV_INPUT + b, "Delay CV");

                    configParam(FEEDBACK_KNOB_PARAM + b, 0, 1, 0, "Feedback", "%", 0, 100);
                    configParam(FEEDBACK_ATTEN_PARAM + b, -1, 1, 0, "Feedback attenuverter", "%", 0, 100);
                    configInput(FEEDBACK_CV_INPUT + b, "Feedback CV");
                }
                initialize();
            }

            void initialize()
            {
                engine.initialize();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void process(const ProcessArgs& args) override
            {
                if (engine.isFinalFrameBeforeBlockChange())
                {
                    for (int b = 0; b < BandCount; ++b)
                    {
                        float level = getControlValue(LEVEL_KNOB_PARAM + b, LEVEL_ATTEN_PARAM + b, LEVEL_CV_INPUT + b, 0, 2);
                        engine.setBandAmplitude(b, level);

                        float dispersion = getControlValue(DISPERSION_KNOB_PARAM + b, DISPERSION_ATTEN_PARAM + b, DISPERSION_CV_INPUT + b, 0, 180);
                        engine.setBandDispersion(b, dispersion);

                        float bandwidth = getControlValue(BANDWIDTH_KNOB_PARAM + b, BANDWIDTH_ATTEN_PARAM + b, BANDWIDTH_CV_INPUT + b, MinBandwidth, MaxBandwidth);
                        engine.setBandWidth(b, bandwidth);

                        float octaves = getControlValue(CENTER_KNOB_PARAM + b, CENTER_ATTEN_PARAM + b, CENTER_CV_INPUT + b, -OctaveHalfRange, +OctaveHalfRange);
                        engine.setCenterFrequencyOffset(b, octaves);
                    }
                }

                // These parameters are not block-based, so they can be evaluated at any time.
                for (int b = 0; b < BandCount; ++b)
                {
                    float delayMillis = getControlValue(DELAY_KNOB_PARAM + b, DELAY_ATTEN_PARAM + b, DELAY_CV_INPUT + b, 0, MaxDelayMillis);
                    engine.setDelay(b, delayMillis);

                    float feedback = getControlValue(FEEDBACK_KNOB_PARAM + b, FEEDBACK_ATTEN_PARAM + b, FEEDBACK_CV_INPUT + b, 0, 1);
                    engine.setFeedback(b, feedback);
                }

                auto &input = inputs[AUDIO_INPUT];
                auto &output = outputs[AUDIO_OUTPUT];

                FrameProcessor::frame_t inFrame;
                inFrame.length = input.getChannels();
                for (int c = 0; c < inFrame.length; ++c)
                    inFrame.data[c] = input.getVoltage(c);

                FrameProcessor::frame_t outFrame;
                engine.process(args.sampleRate, inFrame, outFrame);

                output.setChannels(inFrame.length);
                for (int c = 0; c < outFrame.length; ++c)
                    output.setVoltage(outFrame.data[c], c);
            }
        };


        struct SpatulaWidget : SapphireReloadableModuleWidget
        {
            SpatulaModule *spatulaModule{};

            explicit SpatulaWidget(SpatulaModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/spatula.svg"))
                , spatulaModule(module)
            {
                using string = std::string;

                setModule(module);
                addSapphireInput(AUDIO_INPUT, "audio_input");
                addSapphireOutput(AUDIO_OUTPUT, "audio_output");
                for (int b = 0; b < BandCount; ++b)
                {
                    addSapphireFlatControlGroup(
                        string("center_") + std::to_string(b),
                        CENTER_KNOB_PARAM + b,
                        CENTER_ATTEN_PARAM + b,
                        CENTER_CV_INPUT + b
                    );

                    addSapphireFlatControlGroup(
                        string("level_") + std::to_string(b),
                        LEVEL_KNOB_PARAM + b,
                        LEVEL_ATTEN_PARAM + b,
                        LEVEL_CV_INPUT + b
                    );

                    addSapphireFlatControlGroup(
                        string("dispersion_") + std::to_string(b),
                        DISPERSION_KNOB_PARAM + b,
                        DISPERSION_ATTEN_PARAM + b,
                        DISPERSION_CV_INPUT + b
                    );

                    addSapphireFlatControlGroup(
                        string("bandwidth_") + std::to_string(b),
                        BANDWIDTH_KNOB_PARAM + b,
                        BANDWIDTH_ATTEN_PARAM + b,
                        BANDWIDTH_CV_INPUT + b
                    );

                    addSapphireFlatControlGroup(
                        string("delay_") + std::to_string(b),
                        DELAY_KNOB_PARAM + b,
                        DELAY_ATTEN_PARAM + b,
                        DELAY_CV_INPUT + b
                    );

                    addSapphireFlatControlGroup(
                        string("feedback_") + std::to_string(b),
                        FEEDBACK_KNOB_PARAM + b,
                        FEEDBACK_ATTEN_PARAM + b,
                        FEEDBACK_CV_INPUT + b
                    );
                }
                reloadPanel();
            }
        };
    }
}


Model* modelSpatula = createSapphireModel<Sapphire::Spatula::SpatulaModule, Sapphire::Spatula::SpatulaWidget>(
    "Spatula",
    Sapphire::VectorRole::None
);
