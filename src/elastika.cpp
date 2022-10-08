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
    bool firstTime = true;
    int ballIndex = -1;
    Sapphire::PhysicsVector origin;

public:
    MeshInput()
        : origin(0.0f)
        {}

    MeshInput(int _ballIndex)
        : ballIndex(_ballIndex)
        , origin(0.0f)
        {}

    // Inject audio into the mesh
    void Inject(Sapphire::PhysicsMesh& mesh, const Sapphire::PhysicsVector& direction, float sample)
    {
        using namespace Sapphire;

        Ball& ball = mesh.GetBallAt(ballIndex);
        if (firstTime)
        {
            firstTime = false;
            origin = ball.pos;
        }
        ball.pos = origin + (sample * direction);
    }
};


class MeshOutput
{
private:
    bool firstTime = true;
    int ballIndex;
    Sapphire::PhysicsVector origin;

public:
    MeshOutput()
        : ballIndex(-1)
        , origin(0.0f)
        {}

    MeshOutput(int _ballIndex)
        : ballIndex(_ballIndex)
        , origin(0.0f)
        {}

    // Extract audio from the mesh
    float Extract(Sapphire::PhysicsMesh& mesh, const Sapphire::PhysicsVector& direction)
    {
        using namespace Sapphire;

        Ball& ball = mesh.GetBallAt(ballIndex);
        if (firstTime)
        {
            firstTime = false;
            origin = ball.pos;
        }
        return Dot(ball.pos - origin, direction);
    }
};


struct Elastika : Module
{
    Sapphire::PhysicsMesh mesh;
    SliderMapping frictionMap;
    SliderMapping stiffnessMap;
    SliderMapping spanMap;
    SliderMapping toneMap;
    SliderMapping warpMap;
    MeshInput leftInput;
    MeshInput rightInput;
    MeshOutput leftOutput;
    MeshOutput rightOutput;
    Sapphire::HighPassFilter leftFilter;
    Sapphire::HighPassFilter rightFilter;
    Sapphire::PhysicsVector leftInputDir1;
    Sapphire::PhysicsVector leftInputDir2;
    Sapphire::PhysicsVector rightInputDir1;
    Sapphire::PhysicsVector rightInputDir2;
    Sapphire::PhysicsVector leftOutputDir1;
    Sapphire::PhysicsVector leftOutputDir2;
    Sapphire::PhysicsVector rightOutputDir1;
    Sapphire::PhysicsVector rightOutputDir2;

    enum ParamId
    {
        FRICTION_SLIDER_PARAM,
        STIFFNESS_SLIDER_PARAM,
        SPAN_SLIDER_PARAM,
        TONE_SLIDER_PARAM,
        WARP_SLIDER_PARAM,
        FRICTION_ATTEN_PARAM,
        STIFFNESS_ATTEN_PARAM,
        SPAN_ATTEN_PARAM,
        TONE_ATTEN_PARAM,
        WARP_ATTEN_PARAM,
        DRIVE_KNOB_PARAM,
        LEVEL_KNOB_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        FRICTION_CV_INPUT,
        STIFFNESS_CV_INPUT,
        SPAN_CV_INPUT,
        TONE_CV_INPUT,
        WARP_CV_INPUT,
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
        configParam(WARP_SLIDER_PARAM, 0, 1, 0.5, "Warp control");

        configParam(FRICTION_ATTEN_PARAM, -1, 1, 0, "Friction CV", "%", 0, 100);
        configParam(STIFFNESS_ATTEN_PARAM, -1, 1, 0, "Stiffness CV", "%", 0, 100);
        configParam(SPAN_ATTEN_PARAM, -1, 1, 0, "Spring span CV", "%", 0, 100);
        configParam(TONE_ATTEN_PARAM, -1, 1, 0, "Tone control CV", "%", 0, 100);
        configParam(WARP_ATTEN_PARAM, -1, 1, 0, "Warp control CV", "%", 0, 100);

        configParam(DRIVE_KNOB_PARAM, 0, 2, 1, "Input drive", " dB", -10, 20);
        configParam(LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 20);

        configInput(FRICTION_CV_INPUT, "Friction");
        configInput(STIFFNESS_CV_INPUT, "Stiffness");
        configInput(SPAN_CV_INPUT, "Spring span");
        configInput(TONE_CV_INPUT, "Tone control");
        configInput(WARP_CV_INPUT, "Warp control");

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
        stiffnessMap = SliderMapping(SliderScale::Exponential, {-0.1f, 3.4f});
        spanMap = SliderMapping(SliderScale::Exponential, {-4.14f, 1.3f});
        toneMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});
        warpMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});

        MeshAudioParameters mp = CreateHex(mesh);
        INFO("Mesh has %d balls, %d springs.", mesh.NumBalls(), mesh.NumSprings());

        // Define how stereo inputs go into the mesh.
        leftInput  = MeshInput(mp.leftInputBallIndex);
        rightInput = MeshInput(mp.rightInputBallIndex);
        leftInputDir1 = mp.leftStimulus1;
        leftInputDir2 = mp.leftStimulus2;
        rightInputDir1 = mp.rightStimulus1;
        rightInputDir2 = mp.rightStimulus2;

        // Define how to extract stereo outputs from the mesh.
        leftOutput  = MeshOutput(mp.leftOutputBallIndex);
        rightOutput = MeshOutput(mp.rightOutputBallIndex);
        leftOutputDir1 = mp.leftResponse1;
        leftOutputDir2 = mp.leftResponse2;
        rightOutputDir1 = mp.rightResponse1;
        rightOutputDir2 = mp.rightResponse2;

        leftFilter.Reset();
        rightFilter.Reset();
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        resetState();
    }

    float getControlValue(const SliderMapping& map, ParamId sliderId, ParamId attenuId, InputId cvInputId)
    {
        float slider = params[sliderId].getValue();
        if (inputs[cvInputId].isConnected())
        {
            float attenu = params[attenuId].getValue();
            float cv = inputs[cvInputId].getVoltage();
            slider += attenu * (cv / 10.0f);
        }
        float value = map.Evaluate(clamp(slider));
        return value;
    }

    static Sapphire::PhysicsVector Interpolate(
        float slider,
        const Sapphire::PhysicsVector &a,
        const Sapphire::PhysicsVector &b)
    {
        using namespace Sapphire;

        PhysicsVector c = (1.0-slider)*a + slider*b;

        // Normalize the length to the match the first vector `a`.
        return sqrt(Dot(a,a) / Dot(c,c)) * c;
    }

    void process(const ProcessArgs& args) override
    {
        using namespace Sapphire;

        // Update the mesh parameters from sliders and control voltages.

        float halfLife = getControlValue(frictionMap, FRICTION_SLIDER_PARAM, FRICTION_ATTEN_PARAM, FRICTION_CV_INPUT);
        float restLength = getControlValue(spanMap, SPAN_SLIDER_PARAM, SPAN_ATTEN_PARAM, SPAN_CV_INPUT);
        float stiffness = getControlValue(stiffnessMap, STIFFNESS_SLIDER_PARAM, STIFFNESS_ATTEN_PARAM, STIFFNESS_CV_INPUT);
        float dir = getControlValue(toneMap, TONE_SLIDER_PARAM, TONE_ATTEN_PARAM, TONE_CV_INPUT);
        float warp = getControlValue(warpMap, WARP_SLIDER_PARAM, WARP_ATTEN_PARAM, WARP_CV_INPUT);
        float drive = params[DRIVE_KNOB_PARAM].getValue();
        float gain = params[LEVEL_KNOB_PARAM].getValue();

        mesh.SetRestLength(restLength);
        mesh.SetStiffness(stiffness);

        if (warp >= 0.5)
            mesh.SetMagneticField((warp - 0.5) * PhysicsVector(0.01, 0, 0, 0));
        else
            mesh.SetMagneticField((0.5 - warp) * PhysicsVector(0, 0, 0.01, 0));

        // Feed audio stimulus into the mesh.
        PhysicsVector leftInputDir  = Interpolate(dir, leftInputDir1, leftInputDir2);
        PhysicsVector rightInputDir = Interpolate(dir, rightInputDir1, rightInputDir2);
        leftInput.Inject(mesh, leftInputDir, drive * inputs[AUDIO_LEFT_INPUT].getVoltage());
        rightInput.Inject(mesh, rightInputDir, drive * inputs[AUDIO_RIGHT_INPUT].getVoltage());

        // Update the simulation state by one sample's worth of time.
        mesh.Update(args.sampleTime, halfLife);

        // Extract output for the left channel.
        PhysicsVector leftOutputDir  = Interpolate(dir, leftOutputDir1, leftOutputDir2);
        float lsample = leftOutput.Extract(mesh, leftOutputDir);
        outputs[AUDIO_LEFT_OUTPUT].setVoltage(gain * leftFilter.Update(lsample, args.sampleRate));

        // Extract output for the right channel.
        PhysicsVector rightOutputDir  = Interpolate(dir, rightOutputDir1, rightOutputDir2);
        float rsample = rightOutput.Extract(mesh, rightOutputDir);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(gain * rightFilter.Update(rsample, args.sampleRate));
    }
};


struct ElastikaWidget : ModuleWidget
{
    ElastikaWidget(Elastika* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Sliders
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec( 8.25, 45.94)), module, Elastika::FRICTION_SLIDER_PARAM, Elastika::FRICTION_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(18.37, 45.94)), module, Elastika::STIFFNESS_SLIDER_PARAM, Elastika::STIFFNESS_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(29.57, 45.94)), module, Elastika::SPAN_SLIDER_PARAM, Elastika::SPAN_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(40.99, 45.94)), module, Elastika::TONE_SLIDER_PARAM, Elastika::TONE_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(52.85, 45.94)), module, Elastika::WARP_SLIDER_PARAM, Elastika::TONE_LIGHT));

        // Attenuverters
        addParam(createParamCentered<Trimpot>(mm2px(Vec( 8.25, 71.98)), module, Elastika::FRICTION_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(18.37, 71.98)), module, Elastika::STIFFNESS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(29.57, 71.98)), module, Elastika::SPAN_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(40.99, 71.98)), module, Elastika::TONE_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(52.85, 71.98)), module, Elastika::WARP_ATTEN_PARAM));

        // Drive and Level knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(13.98, 102.08)), module, Elastika::DRIVE_KNOB_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(47.46, 102.08)), module, Elastika::LEVEL_KNOB_PARAM));

        // CV input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 8.25, 81.74)), module, Elastika::FRICTION_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(18.37, 81.74)), module, Elastika::STIFFNESS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(29.57, 81.74)), module, Elastika::SPAN_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(40.99, 81.74)), module, Elastika::TONE_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(52.85, 81.74)), module, Elastika::WARP_CV_INPUT));

        // Audio input Jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 9.12, 113.17)), module, Elastika::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(18.53, 113.17)), module, Elastika::AUDIO_RIGHT_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(42.74, 113.17)), module, Elastika::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(52.15, 113.17)), module, Elastika::AUDIO_RIGHT_OUTPUT));
    }
};


Model* modelElastika = createModel<Elastika, ElastikaWidget>("Elastika");
