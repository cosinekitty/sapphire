#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "filter_tree_16.hpp"

using filter_t = Sapphire::FilterTree_16<float>;
static_assert(filter_t::NBANDS == PORT_MAX_CHANNELS, "FilterTree_16 has wrong number of frequency bands.");

// Sapphire Spectrum for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

struct SpectrumModule : Module
{
    filter_t filter;

    enum ParamId
    {
        PARAMS_LEN
    };

    enum InputId
    {
        AUDIO_INPUT,
        INPUTS_LEN
    };

    enum OutputId
    {
        AUDIO_BANDS_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId
    {
        LIGHTS_LEN
    };

    SpectrumModule()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(AUDIO_INPUT, "Audio");
        configOutput(AUDIO_BANDS_OUTPUT, "Audio bands");
        initialize();
    }

    void initialize()
    {
        filter.reset();
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override
    {
        filter.setSampleRate(e.sampleRate);
    }

    void process(const ProcessArgs& args) override
    {
        float input = inputs[AUDIO_INPUT].getVoltageSum();
        float output[filter_t::NBANDS];
        filter.update(input, output);
        outputs[AUDIO_BANDS_OUTPUT].channels = filter_t::NBANDS;
        outputs[AUDIO_BANDS_OUTPUT].writeVoltages(output);
    }
};


struct SpectrumWidget : SapphireReloadableModuleWidget
{
    SpectrumModule *spectrumModule;

    explicit SpectrumWidget(SpectrumModule *module)
        : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/spectrum.svg"))
        , spectrumModule(module)
    {
        setModule(module);
        addSapphireInput(SpectrumModule::AUDIO_INPUT, "audio_input");
        addSapphireOutput(SpectrumModule::AUDIO_BANDS_OUTPUT, "audio_bands_output");
        reloadPanel();
    }
};


Model* modelSpectrum = createModel<SpectrumModule, SpectrumWidget>("Spectrum");
