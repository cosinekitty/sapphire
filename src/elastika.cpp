#include "plugin.hpp"
#include "mesh.hpp"

// Sapphire Elastika for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

const float OUTPUT_CUTOFF_FREQUENCY = 30.0;

enum class SliderScale
{
    Linear,         // evaluate the polynomial and return the resulting value `y`
    Exponential,    // evaluate the polynomial as `y` and then return 10^y
};


class SliderMapping     // maps a slider value 0..1 onto an arbitrary polynomial expression
{
private:
    SliderScale scale = SliderScale::Linear;
    std::vector<float> polynomial;     // polynomial coefficients, where index = exponent

public:
    SliderMapping() {}

    SliderMapping(SliderScale _scale, std::vector<float> _polynomial)
        : scale(_scale)
        , polynomial(_polynomial)
        {}

    float Evaluate(float x) const
    {
        float y = 0.0;
        float xpower = 1.0;
        for (float coeff : polynomial)
        {
            y += coeff * xpower;
            xpower *= x;
        }

        switch (scale)
        {
        case SliderScale::Exponential:
            return pow(10.0, y);

        case SliderScale::Linear:
        default:
            return y;
        }
    }
};


class MeshInput
{
private:
    int ballIndex = -1;
    Sapphire::PhysicsVector direction;

public:
    MeshInput()
        {}

    MeshInput(int _ballIndex, Sapphire::PhysicsVector _direction)
        : ballIndex(_ballIndex)
        , direction(_direction)
        {}

    // Inject audio into the mesh
    void Inject(Sapphire::PhysicsMesh& mesh, float sample) const
    {
        using namespace Sapphire;

        if (ballIndex >= 0 && ballIndex < mesh.NumBalls())
        {
            Ball& ball = mesh.GetBallAt(ballIndex);
            ball.vel = sample * direction;
        }
    }
};


class MeshOutput
{
private:
    int ballIndex;
    Sapphire::PhysicsVector rdir;   // direction and scale for position component
    Sapphire::PhysicsVector vdir;   // direction and scale for velocity component

public:
    MeshOutput()
        : ballIndex(-1)
        {}

    MeshOutput(int _ballIndex, Sapphire::PhysicsVector _rdir, Sapphire::PhysicsVector _vdir)
        : ballIndex(_ballIndex)
        , rdir(_rdir)
        , vdir(_vdir)
        {}

    // Extract audio from the mesh
    void Extract(Sapphire::PhysicsMesh& mesh, float& rsample, float &vsample) const
    {
        using namespace Sapphire;

        if (ballIndex >= 0 && ballIndex < mesh.NumBalls())
        {
            Ball& ball = mesh.GetBallAt(ballIndex);
            rsample = Dot(ball.pos, rdir);
            vsample = Dot(ball.vel, vdir);
        }
        else
        {
            rsample = 0.0;
            vsample = 0.0;
        }
    }
};


struct Elastika : Module
{
    Sapphire::PhysicsMesh mesh;
    SliderMapping frictionMap;
    SliderMapping stiffnessMap;
    SliderMapping spanMap;
    SliderMapping toneMap;
    MeshInput leftInput;
    MeshInput rightInput;
    MeshOutput leftOutput;
    MeshOutput rightOutput;
    Sapphire::HighPassFilter leftFilter;
    Sapphire::HighPassFilter rightFilter;

    enum ParamId
    {
        FRICTION_SLIDER_PARAM,
        STIFFNESS_SLIDER_PARAM,
        SPAN_SLIDER_PARAM,
        TONE_SLIDER_PARAM,
        FRICTION_ATTEN_PARAM,
        STIFFNESS_ATTEN_PARAM,
        SPAN_ATTEN_PARAM,
        TONE_ATTEN_PARAM,
        LEVEL_KNOB_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        FRICTION_CV_INPUT,
        STIFFNESS_CV_INPUT,
        SPAN_CV_INPUT,
        TONE_CV_INPUT,
        AUDIO_LEFT_INPUT,
        AUDIO_RIGHT_INPUT,
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
        FRICTION_LIGHT,
        STIFFNESS_LIGHT,
        SPAN_LIGHT,
        TONE_LIGHT,
        LIGHTS_LEN
    };

    Elastika()
        : leftFilter(OUTPUT_CUTOFF_FREQUENCY)
        , rightFilter(OUTPUT_CUTOFF_FREQUENCY)
    {
        resetState();

        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(FRICTION_SLIDER_PARAM, 0, 1, 0.5, "Friction");
        configParam(STIFFNESS_SLIDER_PARAM, 0, 1, 0.5, "Stiffness");
        configParam(SPAN_SLIDER_PARAM, 0, 1, 0.5, "Spring span");
        configParam(TONE_SLIDER_PARAM, 0, 1, 0.5, "Tone control");

        configParam(FRICTION_ATTEN_PARAM, -1, 1, 0, "Friction CV", "%", 0, 100);
        configParam(STIFFNESS_ATTEN_PARAM, -1, 1, 0, "Stiffness CV", "%", 0, 100);
        configParam(SPAN_ATTEN_PARAM, -1, 1, 0, "Spring span CV", "%", 0, 100);
        configParam(TONE_ATTEN_PARAM, -1, 1, 0, "Tone control CV", "%", 0, 100);

        configParam(LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 20);

        configInput(FRICTION_CV_INPUT, "Friction");
        configInput(STIFFNESS_CV_INPUT, "Stiffness");
        configInput(SPAN_CV_INPUT, "Spring span");
        configInput(TONE_CV_INPUT, "Tone control");

        configInput(AUDIO_LEFT_INPUT, "Left audio");
        configInput(AUDIO_RIGHT_INPUT, "Right audio");
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

        for (auto& x : lights)
            x.setBrightness(0.3);
    }

    void resetState()
    {
        using namespace Sapphire;

        // Set the defaults for how to interpret the slider values.
        // Determined experimentally to produce useful ranges.

        frictionMap = SliderMapping(SliderScale::Exponential, {1.7f, -5.0f});
        stiffnessMap = SliderMapping(SliderScale::Exponential, {0.0f, 2.5f});
        spanMap = SliderMapping(SliderScale::Exponential, {-4.14f, 1.6f});
        toneMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});

        MeshAudioParameters mp = CreateHex(mesh);
        INFO("Mesh has %d balls, %d springs.", mesh.NumBalls(), mesh.NumSprings());

        // Define how stereo inputs go into the mesh.
        leftInput  = MeshInput(mp.leftInputBallIndex,  mp.leftStimulus);
        rightInput = MeshInput(mp.rightInputBallIndex, mp.rightStimulus);

        // Define how to extract stereo outputs from the mesh.
        float pos_factor = 5.0e+2;
        float vel_factor = 1.0e-1;
        leftOutput  = MeshOutput(mp.leftOutputBallIndex,  pos_factor * mp.leftResponse, vel_factor * mp.leftResponse);
        rightOutput = MeshOutput(mp.rightOutputBallIndex, pos_factor * mp.rightResponse, vel_factor * mp.rightResponse);

        leftFilter.Reset();
        rightFilter.Reset();
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        resetState();
    }

    void process(const ProcessArgs& args) override
    {
        using namespace Sapphire;

        // Update the mesh parameters from sliders and control voltages.
        // FIXFIXFIX: include attenuverter/CV modifications.
        float halfLife = frictionMap.Evaluate(params[FRICTION_SLIDER_PARAM].getValue());
        float restLength = spanMap.Evaluate(params[SPAN_SLIDER_PARAM].getValue());
        float stiffness = stiffnessMap.Evaluate(params[STIFFNESS_SLIDER_PARAM].getValue());

        mesh.SetRestLength(restLength);
        mesh.SetStiffness(stiffness);

        // Feed audio stimulus into the mesh.
        injectAudioChannel(inputs[AUDIO_LEFT_INPUT], leftInput);
        injectAudioChannel(inputs[AUDIO_RIGHT_INPUT], rightInput);

        mesh.Update(args.sampleTime, halfLife);

        float mix = clamp(toneMap.Evaluate(params[TONE_SLIDER_PARAM].getValue()));
        float gain = params[LEVEL_KNOB_PARAM].getValue();
        extractAudioChannel(outputs[AUDIO_LEFT_OUTPUT],  leftOutput,  leftFilter,  args.sampleRate, mix, gain);
        extractAudioChannel(outputs[AUDIO_RIGHT_OUTPUT], rightOutput, rightFilter, args.sampleRate, mix, gain);
    }

    void injectAudioChannel(rack::engine::Input& inp, MeshInput& connect)
    {
        if (inp.isConnected())
            connect.Inject(mesh, inp.getVoltage());
    }

    void extractAudioChannel(
        rack::engine::Output& outp,
        MeshOutput& connect,
        Sapphire::HighPassFilter& filter,
        float sampleRate,
        float mix,
        float gain)
    {
        using namespace Sapphire;

        if (outp.isConnected())
        {
            float rsample, vsample;
            connect.Extract(mesh, rsample, vsample);
            float raw = (1.0 - mix)*rsample + mix*vsample;
            outp.setVoltage(gain * filter.Update(raw, sampleRate));
        }
    }
};


struct ElastikaWidget : ModuleWidget
{
    ElastikaWidget(Elastika* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Sliders
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(10.95, 45.94)), module, Elastika::FRICTION_SLIDER_PARAM, Elastika::FRICTION_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(23.73, 45.94)), module, Elastika::STIFFNESS_SLIDER_PARAM, Elastika::STIFFNESS_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(37.04, 45.94)), module, Elastika::SPAN_SLIDER_PARAM, Elastika::SPAN_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(49.82, 45.94)), module, Elastika::TONE_SLIDER_PARAM, Elastika::TONE_LIGHT));

        // Attenuverters
        addParam(createParamCentered<Trimpot>(mm2px(Vec(10.95, 75.68)), module, Elastika::FRICTION_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(23.73, 75.68)), module, Elastika::STIFFNESS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(37.04, 75.68)), module, Elastika::SPAN_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(49.82, 75.68)), module, Elastika::TONE_ATTEN_PARAM));

        // Level knob
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(30.47, 108.94)), module, Elastika::LEVEL_KNOB_PARAM));

        // CV input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(10.73, 86.50)), module, Elastika::FRICTION_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(23.51, 86.50)), module, Elastika::STIFFNESS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(36.82, 86.50)), module, Elastika::SPAN_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(49.60, 86.50)), module, Elastika::TONE_CV_INPUT));

        // Audio input Jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 9.12, 108.94)), module, Elastika::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(18.53, 108.94)), module, Elastika::AUDIO_RIGHT_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(42.74, 108.94)), module, Elastika::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(52.15, 108.94)), module, Elastika::AUDIO_RIGHT_OUTPUT));
    }
};


Model* modelElastika = createModel<Elastika, ElastikaWidget>("Elastika");
