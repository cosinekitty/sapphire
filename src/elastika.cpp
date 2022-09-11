#include "plugin.hpp"
#include "mesh.hpp"

// Sapphire Elastika for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

enum class SliderScale
{
    Linear,         // evaluate the polynomial and return the resulting value `y`
    Exponential,    // evaluate the polynomial as `y` and then return 10^y
};


static json_t *JsonFromSliderScale(SliderScale scale)
{
    switch (scale)
    {
    case SliderScale::Exponential:
        return json_string("exponential");

    case SliderScale::Linear:
    default:
        return json_string("linear");
    }
}


class SliderMapping     // maps a slider value 0..1 onto an arbitrary polynomial expression
{
private:
    SliderScale scale = SliderScale::Linear;
    std::vector<double> polynomial;     // polynomial coefficients, where index = exponent

public:
    SliderMapping() {}

    SliderMapping(SliderScale _scale, std::vector<double> _polynomial)
        : scale(_scale)
        , polynomial(_polynomial)
        {}

    SliderMapping(json_t *root)
    {
        const char *text = json_string_value(json_object_get(root, "scale"));
        if (text != nullptr && !strcmp(text, "exponential"))
            scale = SliderScale::Exponential;

        json_t *plist = json_object_get(root, "polynomial");
        size_t n = json_array_size(plist);
        for (size_t i = 0; i < n; ++i)
            polynomial.push_back(json_number_value(json_array_get(plist, i)));
    }

    json_t *ToJson() const
    {
        json_t *root = json_object();
        json_object_set_new(root, "scale", JsonFromSliderScale(scale));

        json_t *plist = json_array();
        for (double coeff : polynomial)
            json_array_append(plist, json_real(coeff));
        json_object_set_new(root, "polynomial", plist);

        return root;
    }

    double Evaluate(double x) const
    {
        double y = 0.0;
        double xpower = 1.0;
        for (double coeff : polynomial)
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

    MeshInput(json_t *root)
    {
        ballIndex = json_integer_value(json_object_get(root, "ball"));
        direction = Sapphire::VectorFromJson(root, "dir");
    }

    json_t *ToJson() const
    {
        json_t *root = json_object();
        json_object_set_new(root, "ball", json_integer(ballIndex));
        json_object_set_new(root, "dir", Sapphire::JsonFromVector(direction));
        return root;
    }

    // Inject audio into the mesh
    void Inject(Sapphire::PhysicsMesh& mesh, double sample) const
    {
        using namespace Sapphire;

        if (ballIndex >= 0 && ballIndex < mesh.NumBalls())
        {
            Ball& ball = mesh.GetBallAt(ballIndex);
            ball.vel += sample * direction;
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

    MeshOutput(json_t *root)
    {
        ballIndex = json_integer_value(json_object_get(root, "ball"));
        rdir = Sapphire::VectorFromJson(root, "rdir");
        vdir = Sapphire::VectorFromJson(root, "vdir");
    }

    json_t *ToJson() const
    {
        json_t *root = json_object();
        json_object_set_new(root, "ball", json_integer(ballIndex));
        json_object_set_new(root, "rdir", Sapphire::JsonFromVector(rdir));
        json_object_set_new(root, "vdir", Sapphire::JsonFromVector(vdir));
        return root;
    }

    // Extract audio from the mesh
    void Extract(Sapphire::PhysicsMesh& mesh, double& rsample, double &vsample) const
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


struct ModelNameDisplay : LedDisplay
{
    LedDisplayTextField* textField;

    void init()
    {
        textField = createWidget<LedDisplayTextField>(Vec(0, -4));
        textField->box.size = box.size;
        textField->multiline = false;
        addChild(textField);
    }
};


struct Elastika : Module
{
    Sapphire::PhysicsMesh mesh;
    SliderMapping frictionMap;
    SliderMapping stiffnessMap;
    SliderMapping spanMap;
    SliderMapping toneMap;
    std::string physicsModelName;       // e.g. "drum", "string", ...
    ModelNameDisplay *modelNameDisplay;
    MeshInput leftInput;
    MeshInput rightInput;
    MeshOutput leftOutput;
    MeshOutput rightOutput;

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

        // friction=0 -> halfLife=100; friction=1 -> halfLife=0.01
        // So we want halfLife = 10^(2 - 4*friction)
        frictionMap = SliderMapping(SliderScale::Exponential, {2.0, -4.0});

        // Allow the stiffness to range over 1..1000.
        // So stiffness = 10^(3*slider)
        stiffnessMap = SliderMapping(SliderScale::Exponential, {0.0, 3.0});

        // We want the default span = 0.5 to map to the default rest length.
        // 0.0 -> DEFAULT_REST_LENGTH * 0.1 = 1.0e-4
        // 0.5 -> DEFAULT_REST_LENGTH       = 1.0e-3
        // 1.0 -> DEFAULT_REST_LENGTH * 10  = 1.0e-2
        double spanMiddle = log10(MESH_DEFAULT_REST_LENGTH);
        spanMap = SliderMapping(SliderScale::Exponential, {spanMiddle-1.0, 2.0});

        // Tone map is a simple linear fade-mix between 0..1
        toneMap = SliderMapping(SliderScale::Linear, {0.0, 1.0});

        // Create default mesh configuration
        CreateRoundDrum(mesh, 8);
        physicsModelName = "drum";

        // Define how stereo inputs go into the mesh.
        leftInput  = MeshInput(64, PhysicsVector(0.01, 0.01, 0.05));
        rightInput = MeshInput(70, PhysicsVector(0.01, 0.01, 0.05));

        // Define how to extract stereo outputs from the mesh.
        leftOutput  = MeshOutput(113, PhysicsVector(5, 5, 5), 12.0*PhysicsVector(5, 5, 5));
        rightOutput = MeshOutput(115, PhysicsVector(5, 5, 5), 12.0*PhysicsVector(5, 5, 5));
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        resetState();
    }

    void process(const ProcessArgs& args) override
    {
        // Update the mesh parameters from sliders and control voltages.
        double halfLife = frictionMap.Evaluate(params[FRICTION_SLIDER_PARAM].getValue());
        double restLength = spanMap.Evaluate(params[SPAN_SLIDER_PARAM].getValue());
        double stiffness = stiffnessMap.Evaluate(params[STIFFNESS_SLIDER_PARAM].getValue());

        mesh.SetRestLength(restLength);
        mesh.SetStiffness(stiffness);

        // Feed audio stimulus into the mesh.
        injectAudioChannel(inputs[AUDIO_LEFT_INPUT], leftInput);
        injectAudioChannel(inputs[AUDIO_RIGHT_INPUT], rightInput);

        mesh.Update(args.sampleTime, halfLife);

        float gain = params[LEVEL_KNOB_PARAM].getValue();
        extractAudioChannel(outputs[AUDIO_LEFT_OUTPUT], leftOutput, gain);
        extractAudioChannel(outputs[AUDIO_RIGHT_OUTPUT], rightOutput, gain);
    }

    void injectAudioChannel(rack::engine::Input& inp, MeshInput& connect)
    {
        if (inp.isConnected())
            connect.Inject(mesh, inp.getVoltage());
    }

    void extractAudioChannel(rack::engine::Output& outp, MeshOutput& connect, float gain)
    {
        if (outp.isConnected())
        {
            double rsample, vsample;
            connect.Extract(mesh, rsample, vsample);
            outp.setChannels(1);

            double mix = clamp(toneMap.Evaluate(params[TONE_SLIDER_PARAM].getValue()));
            double raw = (1.0 - mix)*rsample + mix*vsample;
            outp.setVoltage(gain * raw);
        }
    }

    json_t* dataToJson() override
    {
        json_t* root = json_object();

        json_t* map = json_object();
        json_object_set_new(map, "friction", frictionMap.ToJson());
        json_object_set_new(map, "stiffness", stiffnessMap.ToJson());
        json_object_set_new(map, "span", spanMap.ToJson());
        json_object_set_new(map, "tone", toneMap.ToJson());
        json_object_set_new(root, "sliders", map);

        json_t *inputArray = json_array();
        json_array_append(inputArray, leftInput.ToJson());
        json_array_append(inputArray, rightInput.ToJson());
        json_object_set_new(root, "inputs", inputArray);

        json_t *outputArray = json_array();
        json_array_append(outputArray, leftOutput.ToJson());
        json_array_append(outputArray, rightOutput.ToJson());
        json_object_set_new(root, "outputs", outputArray);

        json_object_set_new(root, "mesh", JsonFromMesh(mesh));

        return root;
    }

    void dataFromJson(json_t* root) override
    {
        Sapphire::MeshFromJson(mesh, json_object_get(root, "mesh"));

        json_t* sliders = json_object_get(root, "sliders");
        if (json_is_object(sliders))
        {
            frictionMap = SliderMapping(json_object_get(sliders, "friction"));
            stiffnessMap = SliderMapping(json_object_get(sliders, "stiffness"));
            spanMap = SliderMapping(json_object_get(sliders, "span"));
            toneMap = SliderMapping(json_object_get(sliders, "tone"));
        }

        leftInput = MeshInput();
        rightInput = MeshInput();
        json_t *inputArray = json_object_get(root, "inputs");
        if (json_array_size(inputArray) == 2)
        {
            leftInput = MeshInput(json_array_get(inputArray, 0));
            rightInput = MeshInput(json_array_get(inputArray, 1));
        }

        leftOutput = MeshOutput();
        rightOutput = MeshOutput();
        json_t *outputArray = json_object_get(root, "outputs");
        if (json_array_size(outputArray) == 2)
        {
            leftOutput = MeshOutput(json_array_get(outputArray, 0));
            rightOutput = MeshOutput(json_array_get(outputArray, 1));
        }
    }
};


struct ElastikaWidget : ModuleWidget
{
    ElastikaWidget(Elastika* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Text edit box for displaying/editing the physics model name.
        ModelNameDisplay* textDisplay = createWidget<ModelNameDisplay>(mm2px(Vec(21.625443, 12.590074)));

        textDisplay->box.size = mm2px(Vec(32.653057, 7.4503818));
        textDisplay->init();
        addChild(textDisplay);

        if (module != nullptr)      // module can be null when previewing in the picker
        {
            module->modelNameDisplay = textDisplay;
            textDisplay->textField->text = module->physicsModelName;    // FIXFIXFIX: keep up-to-date
        }

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

