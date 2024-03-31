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
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_INPUT,
            ENUMS(LEVEL_CV_INPUT, BandCount),
            ENUMS(DISPERSION_CV_INPUT, BandCount),
            ENUMS(BANDWIDTH_CV_INPUT, BandCount),
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
                    configParam(LEVEL_KNOB_PARAM + b, 0, 2, 1, "Band level", " dB", -10, 20*OUTPUT_EXPONENT);
                    configParam(LEVEL_ATTEN_PARAM + b, -1, 1, 0, "Band level attenuverter", "%", 0, 100);
                    configInput(LEVEL_CV_INPUT + b, "Band level CV");

                    configParam(DISPERSION_KNOB_PARAM + b, 0, 180, 0, "Dispersion", "°");
                    configParam(DISPERSION_ATTEN_PARAM + b, -1, 1, 0, "Dispersion attenuverter", "%", 0, 100);
                    configInput(DISPERSION_CV_INPUT + b, "Dispersion CV");

                    configParam(BANDWIDTH_KNOB_PARAM + b, MinBandwidth, MaxBandwidth, 0, "Bandwidth", "");
                    configParam(BANDWIDTH_ATTEN_PARAM + b, -1, 1, 0, "Bandwidth attenuverter", "%", 0, 100);
                    configInput(BANDWIDTH_CV_INPUT + b, "Bandwidth CV");
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
                    }
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
