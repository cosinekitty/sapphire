#include "plugin.hpp"
#include "mesh.hpp"

// Sapphire Elastika for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

const int ELASTIKA_FILTER_LAYERS = 3;

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
    Slewer slewer;
    bool isPowerGateActive;
    Sapphire::PhysicsMesh mesh;
    SliderMapping frictionMap;
    SliderMapping stiffnessMap;
    SliderMapping spanMap;
    SliderMapping curlMap;
    SliderMapping tiltMap;
    MeshInput leftInput;
    MeshInput rightInput;
    MeshOutput leftOutput;
    MeshOutput rightOutput;
    Sapphire::StagedFilter<ELASTIKA_FILTER_LAYERS> leftLoCut;
    Sapphire::StagedFilter<ELASTIKA_FILTER_LAYERS> leftHiCut;
    Sapphire::StagedFilter<ELASTIKA_FILTER_LAYERS> rightLoCut;
    Sapphire::StagedFilter<ELASTIKA_FILTER_LAYERS> rightHiCut;
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
        CURL_SLIDER_PARAM,
        TILT_SLIDER_PARAM,
        FRICTION_ATTEN_PARAM,
        STIFFNESS_ATTEN_PARAM,
        SPAN_ATTEN_PARAM,
        CURL_ATTEN_PARAM,
        TILT_ATTEN_PARAM,
        DRIVE_KNOB_PARAM,
        LEVEL_KNOB_PARAM,
        LO_CUT_KNOB_PARAM,
        HI_CUT_KNOB_PARAM,
        POWER_TOGGLE_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        FRICTION_CV_INPUT,
        STIFFNESS_CV_INPUT,
        SPAN_CV_INPUT,
        CURL_CV_INPUT,
        TILT_CV_INPUT,
        AUDIO_LEFT_INPUT,
        AUDIO_RIGHT_INPUT,
        POWER_GATE_INPUT,
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
        CURL_LIGHT,
        TILT_LIGHT,
        POWER_LIGHT,
        LIGHTS_LEN
    };

    Elastika()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(FRICTION_SLIDER_PARAM, 0, 1, 0.5, "Friction");
        configParam(STIFFNESS_SLIDER_PARAM, 0, 1, 0.5, "Stiffness");
        configParam(SPAN_SLIDER_PARAM, 0, 1, 0.5, "Spring span");
        configParam(CURL_SLIDER_PARAM, -1, +1, 0, "Magnetic field");
        configParam(TILT_SLIDER_PARAM, 0, 1, 0.5, "Tilt angle");

        configParam(FRICTION_ATTEN_PARAM, -1, 1, 0, "Friction", "%", 0, 100);
        configParam(STIFFNESS_ATTEN_PARAM, -1, 1, 0, "Stiffness", "%", 0, 100);
        configParam(SPAN_ATTEN_PARAM, -1, 1, 0, "Spring span", "%", 0, 100);
        configParam(CURL_ATTEN_PARAM, -1, 1, 0, "Magnetic field", "%", 0, 100);
        configParam(TILT_ATTEN_PARAM, -1, 1, 0, "Tilt angle", "%", 0, 100);

        configParam(DRIVE_KNOB_PARAM, 0, 2, 1, "Input drive", " dB", -10, 80);
        configParam(LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 80);
        configParam(LO_CUT_KNOB_PARAM, 1, 4, 1, "Low cutoff", " Hz", +10, 1, 0);
        configParam(HI_CUT_KNOB_PARAM, 1, 4, 4, "High cutoff", " Hz", +10, 1, 0);

        configInput(FRICTION_CV_INPUT, "Friction CV");
        configInput(STIFFNESS_CV_INPUT, "Stiffness CV");
        configInput(SPAN_CV_INPUT, "Spring span CV");
        configInput(CURL_CV_INPUT, "Magnetic field CV");
        configInput(TILT_CV_INPUT, "Tilt angle CV");

        configInput(AUDIO_LEFT_INPUT, "Left audio");
        configInput(AUDIO_RIGHT_INPUT, "Right audio");
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

        configButton(POWER_TOGGLE_PARAM, "Power");
        configInput(POWER_GATE_INPUT, "Power gate");

        for (auto& x : lights)
            x.setBrightness(0.3);

        initialize();
    }

    void initialize()
    {
        using namespace Sapphire;

        isPowerGateActive = true;
        slewer.enable(true);
        params[POWER_TOGGLE_PARAM].setValue(1.0f);

        // Set the defaults for how to interpret the slider values.
        // Determined experimentally to produce useful ranges.

        frictionMap = SliderMapping(SliderScale::Exponential, {1.3f, -4.5f});
        stiffnessMap = SliderMapping(SliderScale::Exponential, {-0.1f, 3.4f});
        spanMap = SliderMapping(SliderScale::Linear, {0.0008, 0.0003});
        curlMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});
        tiltMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});

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

        leftLoCut.Reset();
        leftHiCut.Reset();
        rightLoCut.Reset();
        rightHiCut.Reset();
    }

    void onReset(const ResetEvent& e) override
    {
        Module::onReset(e);
        initialize();
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override
    {
        // We slew using a linear ramp over a time span of 1/400 of a second.
        // Round to the nearest integer number of samples for the current sample rate.
        int newRampLength = static_cast<int>(round(e.sampleRate / 400.0f));
        slewer.setRampLength(newRampLength);
    }

    float getControlValue(
        const SliderMapping& map,
        ParamId sliderId,
        ParamId attenuId,
        InputId cvInputId,
        float minSlider = 0.0f,
        float maxSlider = 1.0f)
    {
        float slider = params[sliderId].getValue();
        if (inputs[cvInputId].isConnected())
        {
            float attenu = params[attenuId].getValue();
            float cv = inputs[cvInputId].getVoltage();
            slider += attenu * (cv / 10.0f);
        }
        float value = map.Evaluate(clamp(slider, minSlider, maxSlider));
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

        // The user is allowed to turn off Elastika to reduce CPU usage.
        // Check the gate input voltage first, and debounce it.
        // If the gate is not connected, fall back to the pushbutton state.
        auto& gate = inputs[POWER_GATE_INPUT];
        if (gate.isConnected())
        {
            // If the gate input is connected, use the voltage of its first channel
            // to control whether POWER is enabled or disabled.
            // Debounce the signal using hysteresis like a Schmitt trigger would.
            // See: https://vcvrack.com/manual/VoltageStandards#Triggers-and-Gates
            const float gv = gate.getVoltage();
            if (isPowerGateActive)
            {
                if (gv <= 0.1f)
                    isPowerGateActive = false;
            }
            else
            {
                if (gv >= 1.0f)
                    isPowerGateActive = true;
            }
        }
        else
        {
            // When no gate input is connected, allow the manual pushbutton take control.
            isPowerGateActive = (params[POWER_TOGGLE_PARAM].getValue() > 0.0f);
        }

        // Set the pushbutton illumination to track the power state,
        // whether the power state was set by the button itself or the power gate.
        lights[POWER_LIGHT].setBrightness(isPowerGateActive ? 1.0f : 0.03f);

        if (!slewer.update(isPowerGateActive))
        {
            // Output silent stereo signal without using any more CPU.
            outputs[AUDIO_LEFT_OUTPUT].setVoltage(0.0f);
            outputs[AUDIO_RIGHT_OUTPUT].setVoltage(0.0f);
            return;
        }

        // Update the mesh parameters from sliders and control voltages.

        float halfLife = getControlValue(frictionMap, FRICTION_SLIDER_PARAM, FRICTION_ATTEN_PARAM, FRICTION_CV_INPUT);
        float restLength = getControlValue(spanMap, SPAN_SLIDER_PARAM, SPAN_ATTEN_PARAM, SPAN_CV_INPUT);
        float stiffness = getControlValue(stiffnessMap, STIFFNESS_SLIDER_PARAM, STIFFNESS_ATTEN_PARAM, STIFFNESS_CV_INPUT);
        float curl = getControlValue(curlMap, CURL_SLIDER_PARAM, CURL_ATTEN_PARAM, CURL_CV_INPUT, -1.0f, +1.0f);
        float tilt = getControlValue(tiltMap, TILT_SLIDER_PARAM, TILT_ATTEN_PARAM, TILT_CV_INPUT);
        float drive = std::pow(params[DRIVE_KNOB_PARAM].getValue(), 4.0f);
        float gain = std::pow(params[LEVEL_KNOB_PARAM].getValue(), 4.0f);
        float loCutHz = std::pow(10.0f, params[LO_CUT_KNOB_PARAM].getValue());
        float hiCutHz = std::pow(10.0f, params[HI_CUT_KNOB_PARAM].getValue());

        leftLoCut.SetCutoffFrequency(loCutHz);
        rightLoCut.SetCutoffFrequency(loCutHz);
        leftHiCut.SetCutoffFrequency(hiCutHz);
        rightHiCut.SetCutoffFrequency(hiCutHz);

        mesh.SetRestLength(restLength);
        mesh.SetStiffness(stiffness);

        if (curl >= 0.0f)
            mesh.SetMagneticField(curl * PhysicsVector(0.005, 0, 0, 0));
        else
            mesh.SetMagneticField(curl * PhysicsVector(0, 0, -0.005, 0));

        // Feed audio stimulus into the mesh.
        PhysicsVector leftInputDir = Interpolate(tilt, leftInputDir1, leftInputDir2);
        PhysicsVector rightInputDir = Interpolate(tilt, rightInputDir1, rightInputDir2);
        leftInput.Inject(mesh, leftInputDir, drive * inputs[AUDIO_LEFT_INPUT].getVoltage());
        rightInput.Inject(mesh, rightInputDir, drive * inputs[AUDIO_RIGHT_INPUT].getVoltage());

        // Update the simulation state by one sample's worth of time.
        mesh.Update(args.sampleTime, halfLife);

        // Extract output for the left channel.
        PhysicsVector leftOutputDir = Interpolate(tilt, leftOutputDir1, leftOutputDir2);
        float lsample = leftOutput.Extract(mesh, leftOutputDir);
        lsample = leftLoCut.UpdateHiPass(lsample, args.sampleRate);
        lsample = leftHiCut.UpdateLoPass(lsample, args.sampleRate);
        outputs[AUDIO_LEFT_OUTPUT].setVoltage(gain * lsample);

        // Extract output for the right channel.
        PhysicsVector rightOutputDir = Interpolate(tilt, rightOutputDir1, rightOutputDir2);
        float rsample = rightOutput.Extract(mesh, rightOutputDir);
        rsample = rightLoCut.UpdateHiPass(rsample, args.sampleRate);
        rsample = rightHiCut.UpdateLoPass(rsample, args.sampleRate);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(gain * rsample);
    }
};


struct ElastikaWidget : ModuleWidget
{
    ElastikaWidget(Elastika* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Sliders
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec( 8.00, 46.00)), module, Elastika::FRICTION_SLIDER_PARAM, Elastika::FRICTION_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(19.24, 46.00)), module, Elastika::STIFFNESS_SLIDER_PARAM, Elastika::STIFFNESS_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(30.48, 46.00)), module, Elastika::SPAN_SLIDER_PARAM, Elastika::SPAN_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(41.72, 46.00)), module, Elastika::CURL_SLIDER_PARAM, Elastika::CURL_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(52.96, 46.00)), module, Elastika::TILT_SLIDER_PARAM, Elastika::TILT_LIGHT));

        // Attenuverters
        addParam(createParamCentered<Trimpot>(mm2px(Vec( 8.00, 72.00)), module, Elastika::FRICTION_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(19.24, 72.00)), module, Elastika::STIFFNESS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48, 72.00)), module, Elastika::SPAN_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(41.72, 72.00)), module, Elastika::CURL_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(52.96, 72.00)), module, Elastika::TILT_ATTEN_PARAM));

        // Drive and Level knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(14.00, 102.00)), module, Elastika::DRIVE_KNOB_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(46.96, 102.00)), module, Elastika::LEVEL_KNOB_PARAM));

        // Cutoff filter knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20.32, 20.00)), module, Elastika::LO_CUT_KNOB_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(40.64, 20.00)), module, Elastika::HI_CUT_KNOB_PARAM));

        // CV input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 8.00, 81.74)), module, Elastika::FRICTION_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(19.24, 81.74)), module, Elastika::STIFFNESS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(30.48, 81.74)), module, Elastika::SPAN_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(41.72, 81.74)), module, Elastika::CURL_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(52.96, 81.74)), module, Elastika::TILT_CV_INPUT));

        // Audio input Jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 7.50, 115.00)), module, Elastika::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(20.50, 115.00)), module, Elastika::AUDIO_RIGHT_INPUT));

        // Audio output jacks
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(40.46, 115.00)), module, Elastika::AUDIO_LEFT_OUTPUT));
        addOutput(createOutputCentered<SapphirePort>(mm2px(Vec(53.46, 115.00)), module, Elastika::AUDIO_RIGHT_OUTPUT));

        // Power enable/disable
        addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(30.48, 95.0)), module, Elastika::POWER_TOGGLE_PARAM, Elastika::POWER_LIGHT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(30.48, 104.0)), module, Elastika::POWER_GATE_INPUT));
    }
};


Model* modelElastika = createModel<Elastika, ElastikaWidget>("Elastika");
