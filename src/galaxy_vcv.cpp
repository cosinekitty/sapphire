#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "galaxy_engine.hpp"

// Sapphire Galaxy for VCV Rack by Don Cross <cosinekitty@gmail.com>
// Part of the Sapphire project at:
// https://github.com/cosinekitty/sapphire
//
// An adaptation of Airwindows Galactic by Chris Johnson:
// https://github.com/airwindows/airwindows

namespace Sapphire
{
    namespace Galaxy
    {
        enum ParamId
        {
            REPLACE_PARAM,
            REPLACE_ATTEN,
            BRIGHTNESS_PARAM,
            BRIGHTNESS_ATTEN,
            DETUNE_PARAM,
            DETUNE_ATTEN,
            BIGNESS_PARAM,
            BIGNESS_ATTEN,
            MIX_PARAM,
            MIX_ATTEN,
            PARAMS_LEN
        };

        enum InputId
        {
            AUDIO_LEFT_INPUT,
            AUDIO_RIGHT_INPUT,
            REPLACE_CV_INPUT,
            BRIGHTNESS_CV_INPUT,
            DETUNE_CV_INPUT,
            BIGNESS_CV_INPUT,
            MIX_CV_INPUT,
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

        struct GalaxyModule : SapphireModule
        {
            Engine engine;
            bool enableStereoSplitter{};
            float autoResetVoltageThreshold = 100;
            int autoResetCountdown = 0;

            GalaxyModule()
                : SapphireModule(PARAMS_LEN)
            {
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

                configInput(AUDIO_LEFT_INPUT, "Audio left");
                configInput(AUDIO_RIGHT_INPUT, "Audio right");

                configOutput(AUDIO_LEFT_OUTPUT, "Audio left");
                configOutput(AUDIO_RIGHT_OUTPUT, "Audio right");

                configGalaxyGroup("Replace", REPLACE_PARAM, REPLACE_ATTEN, REPLACE_CV_INPUT);
                configGalaxyGroup("Brightness", BRIGHTNESS_PARAM, BRIGHTNESS_ATTEN, BRIGHTNESS_CV_INPUT);
                configGalaxyGroup("Detune", DETUNE_PARAM, DETUNE_ATTEN, DETUNE_CV_INPUT);
                configGalaxyGroup("Size", BIGNESS_PARAM, BIGNESS_ATTEN, BIGNESS_CV_INPUT);
                configGalaxyGroup("Mix", MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);

                configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
                configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);

                initialize();
            }

            void configGalaxyGroup(const std::string& name, int paramId, int attenId, int cvInputId)
            {
                configControlGroup(name, paramId, attenId, cvInputId, ParamKnobMin, ParamKnobMax, ParamKnobDef);
            }

            void initialize()
            {
                engine.initialize();
                enableStereoSplitter = false;
                autoResetCountdown = 0;
            }

            json_t* dataToJson() override
            {
                json_t* root = SapphireModule::dataToJson();
                json_object_set_new(root, "enableStereoSplitter", json_boolean(enableStereoSplitter));
                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);
                json_t *flag = json_object_get(root, "enableStereoSplitter");
                enableStereoSplitter = json_is_true(flag);
            }

            void onReset(const ResetEvent& e) override
            {
                SapphireModule::onReset(e);
                initialize();
            }

            void loadInputs(float& inLeft, float& inRight)
            {
                const int ncl = inputs[AUDIO_LEFT_INPUT ].channels;
                const int ncr = inputs[AUDIO_RIGHT_INPUT].channels;

                if (enableStereoSplitter)
                {
                    // Special option: stereo given to either input port,
                    // but with the other port disconnected, results in a stereo splitter.

                    if (ncl >= 2 && ncr == 0)
                    {
                        inLeft  = inputs[AUDIO_LEFT_INPUT].getVoltage(0);
                        inRight = inputs[AUDIO_LEFT_INPUT].getVoltage(1);
                        return;
                    }

                    if (ncr >= 2 && ncl == 0)
                    {
                        inLeft  = inputs[AUDIO_RIGHT_INPUT].getVoltage(0);
                        inRight = inputs[AUDIO_RIGHT_INPUT].getVoltage(1);
                        return;
                    }
                }

                // Assume separate data fed to each input port.

                inLeft  = inputs[AUDIO_LEFT_INPUT ].getVoltageSum();
                inRight = inputs[AUDIO_RIGHT_INPUT].getVoltageSum();

                // But if only one of the two input ports has a cable,
                // split that cable's voltage equally between the left and right inputs.
                // This is "mono" mode.

                if (ncl > 0 && ncr == 0)
                    inLeft = inRight = inLeft / 2;
                else if (ncr > 0 && ncl == 0)
                    inLeft = inRight = inRight / 2;
            }

            bool isBadOutput(float output) const
            {
                return !std::isfinite(output) || std::abs(output) > autoResetVoltageThreshold;
            }

            void process(const ProcessArgs& args) override
            {
                float outLeft, outRight;
                if (autoResetCountdown > 0)
                {
                    // Continue to silence the output for the remainder of the reset period.
                    --autoResetCountdown;
                    outLeft = outRight = 0;
                }
                else
                {
                    engine.setReplace(getControlValue(REPLACE_PARAM, REPLACE_ATTEN, REPLACE_CV_INPUT));
                    engine.setBrightness(getControlValue(BRIGHTNESS_PARAM, BRIGHTNESS_ATTEN, BRIGHTNESS_CV_INPUT));
                    engine.setDetune(getControlValue(DETUNE_PARAM, DETUNE_ATTEN, DETUNE_CV_INPUT));
                    engine.setBigness(getControlValue(BIGNESS_PARAM, BIGNESS_ATTEN, BIGNESS_CV_INPUT));
                    engine.setMix(getControlValue(MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT));

                    float inLeft, inRight;
                    loadInputs(inLeft, inRight);

                    engine.process(args.sampleRate, inLeft, inRight, outLeft, outRight);

                    // Is the output getting out of control? Or even NAN?
                    if (isBadOutput(outLeft) || isBadOutput(outRight))
                    {
                        // Reset the engine (which also initializes `autoResetCountdown`.)
                        engine.initialize();

                        // Silence the output for 1/4 of a second.
                        autoResetCountdown = static_cast<int>(round(args.sampleRate / 4));

                        // Start the silence on this sample.
                        outLeft = outRight = 0;
                    }
                }
                outputs[AUDIO_LEFT_OUTPUT ].setVoltage(outLeft);
                outputs[AUDIO_RIGHT_OUTPUT].setVoltage(outRight);
            }
        };

        struct GalaxyWidget : SapphireReloadableModuleWidget
        {
            GalaxyModule* galaxyModule{};

            explicit GalaxyWidget(GalaxyModule* module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/galaxy.svg"))
                , galaxyModule(module)
            {
                setModule(module);

                addSapphireInput(AUDIO_LEFT_INPUT, "audio_left_input");
                addSapphireInput(AUDIO_RIGHT_INPUT, "audio_right_input");

                addSapphireOutput(AUDIO_LEFT_OUTPUT, "audio_left_output");
                addSapphireOutput(AUDIO_RIGHT_OUTPUT, "audio_right_output");

                addSapphireFlatControlGroup("replace", REPLACE_PARAM, REPLACE_ATTEN, REPLACE_CV_INPUT);
                addSapphireFlatControlGroup("brightness", BRIGHTNESS_PARAM, BRIGHTNESS_ATTEN, BRIGHTNESS_CV_INPUT);
                addSapphireFlatControlGroup("detune", DETUNE_PARAM, DETUNE_ATTEN, DETUNE_CV_INPUT);
                addSapphireFlatControlGroup("bigness", BIGNESS_PARAM, BIGNESS_ATTEN, BIGNESS_CV_INPUT);
                addSapphireFlatControlGroup("mix", MIX_PARAM, MIX_ATTEN, MIX_CV_INPUT);

                reloadPanel();
            }

            void appendContextMenu(Menu* menu) override
            {
                if (galaxyModule == nullptr)
                    return;

                menu->addChild(new MenuSeparator);
                menu->addChild(galaxyModule->createToggleAllSensitivityMenuItem());
                menu->addChild(createBoolPtrMenuItem<bool>("Enable input stereo splitter", "", &galaxyModule->enableStereoSplitter));
            }
        };
    }
}

Model *modelSapphireGalaxy = createSapphireModel<Sapphire::Galaxy::GalaxyModule, Sapphire::Galaxy::GalaxyWidget>(
    "Galaxy",
    Sapphire::VectorRole::None
);
