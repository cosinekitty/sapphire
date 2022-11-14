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


class SliderMapping     // maps a slider value onto an arbitrary polynomial expression
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


class MeshInput     // facilitates injecting audio into the mesh
{
private:
    int ballIndex;

public:
    MeshInput()
        : ballIndex(-1)
        {}

    explicit MeshInput(int _ballIndex)
        : ballIndex(_ballIndex)
        {}

    // Inject audio into the mesh
    void Inject(Sapphire::PhysicsMesh& mesh, const Sapphire::PhysicsVector& direction, float sample)
    {
        Sapphire::Ball& ball = mesh.GetBallAt(ballIndex);
        ball.pos = mesh.GetBallOrigin(ballIndex) + (sample * direction);
    }
};


class MeshOutput    // facilitates extracting audio from the mesh
{
private:
    int ballIndex;

public:
    MeshOutput()
        : ballIndex(-1)
        {}

    explicit MeshOutput(int _ballIndex)
        : ballIndex(_ballIndex)
        {}

    // Extract audio from the mesh
    float Extract(Sapphire::PhysicsMesh& mesh, const Sapphire::PhysicsVector& direction)
    {
        using namespace Sapphire;

        PhysicsVector movement = mesh.GetBallDisplacement(ballIndex);
        return Dot(movement, direction);
    }
};


struct Elastika : Module
{
    Sapphire::Slewer slewer;
    bool isPowerGateActive;
    bool isQuiet;
    int outputVerifyCounter;
    Sapphire::PhysicsMesh mesh;
    Sapphire::MeshAudioParameters mp;
    SliderMapping frictionMap;
    SliderMapping stiffnessMap;
    SliderMapping spanMap;
    SliderMapping curlMap;
    SliderMapping massMap;
    SliderMapping tiltMap;
    MeshInput leftInput;
    MeshInput rightInput;
    MeshOutput leftOutput;
    MeshOutput rightOutput;
    Sapphire::StagedFilter<ELASTIKA_FILTER_LAYERS> leftLoCut;
    Sapphire::StagedFilter<ELASTIKA_FILTER_LAYERS> rightLoCut;
    DcRejectQuantity *dcRejectQuantity = nullptr;

    enum ParamId
    {
        FRICTION_SLIDER_PARAM,
        STIFFNESS_SLIDER_PARAM,
        SPAN_SLIDER_PARAM,
        CURL_SLIDER_PARAM,
        MASS_SLIDER_PARAM,
        FRICTION_ATTEN_PARAM,
        STIFFNESS_ATTEN_PARAM,
        SPAN_ATTEN_PARAM,
        CURL_ATTEN_PARAM,
        MASS_ATTEN_PARAM,
        DRIVE_KNOB_PARAM,
        LEVEL_KNOB_PARAM,
        INPUT_TILT_KNOB_PARAM,
        OUTPUT_TILT_KNOB_PARAM,
        POWER_TOGGLE_PARAM,
        INPUT_TILT_ATTEN_PARAM,
        OUTPUT_TILT_ATTEN_PARAM,
        DC_REJECT_PARAM,
        PARAMS_LEN
    };

    enum InputId
    {
        FRICTION_CV_INPUT,
        STIFFNESS_CV_INPUT,
        SPAN_CV_INPUT,
        CURL_CV_INPUT,
        MASS_CV_INPUT,
        AUDIO_LEFT_INPUT,
        AUDIO_RIGHT_INPUT,
        POWER_GATE_INPUT,
        INPUT_TILT_CV_INPUT,
        OUTPUT_TILT_CV_INPUT,
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
        MASS_LIGHT,
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
        configParam(MASS_SLIDER_PARAM, -1, +1, 0, "Impurity mass", "", 10, 1);

        configParam(FRICTION_ATTEN_PARAM, -1, 1, 0, "Friction", "%", 0, 100);
        configParam(STIFFNESS_ATTEN_PARAM, -1, 1, 0, "Stiffness", "%", 0, 100);
        configParam(SPAN_ATTEN_PARAM, -1, 1, 0, "Spring span", "%", 0, 100);
        configParam(CURL_ATTEN_PARAM, -1, 1, 0, "Magnetic field", "%", 0, 100);
        configParam(MASS_ATTEN_PARAM, -1, 1, 0, "Impurity mass", "%", 0, 100);
        configParam(INPUT_TILT_ATTEN_PARAM, -1, 1, 0, "Input tilt angle", "%", 0, 100);
        configParam(OUTPUT_TILT_ATTEN_PARAM, -1, 1, 0, "Output tilt angle", "%", 0, 100);
        configParam<DcRejectQuantity>(DC_REJECT_PARAM, 20, 400, 20, "DC reject cutoff", " Hz");
        dcRejectQuantity = dynamic_cast<DcRejectQuantity *>(paramQuantities[DC_REJECT_PARAM]);

        auto driveKnob = configParam(DRIVE_KNOB_PARAM, 0, 2, 1, "Input drive", " dB", -10, 80);
        auto levelKnob = configParam(LEVEL_KNOB_PARAM, 0, 2, 1, "Output level", " dB", -10, 80);
        configParam(INPUT_TILT_KNOB_PARAM,  0, 1, 0.5, "Input tilt angle", "°", 0, 90);
        configParam(OUTPUT_TILT_KNOB_PARAM, 0, 1, 0.5, "Output tilt angle", "°", 0, 90);

        configInput(FRICTION_CV_INPUT, "Friction CV");
        configInput(STIFFNESS_CV_INPUT, "Stiffness CV");
        configInput(SPAN_CV_INPUT, "Spring span CV");
        configInput(CURL_CV_INPUT, "Magnetic field CV");
        configInput(MASS_CV_INPUT, "Impurity mass CV");
        configInput(INPUT_TILT_CV_INPUT, "Input tilt CV");
        configInput(OUTPUT_TILT_CV_INPUT, "Output tilt CV");

        configInput(AUDIO_LEFT_INPUT, "Left audio");
        configInput(AUDIO_RIGHT_INPUT, "Right audio");
        configOutput(AUDIO_LEFT_OUTPUT, "Left audio");
        configOutput(AUDIO_RIGHT_OUTPUT, "Right audio");

        configButton(POWER_TOGGLE_PARAM, "Power");
        configInput(POWER_GATE_INPUT, "Power gate");

        configBypass(AUDIO_LEFT_INPUT, AUDIO_LEFT_OUTPUT);
        configBypass(AUDIO_RIGHT_INPUT, AUDIO_RIGHT_OUTPUT);

        for (auto& x : lights)
            x.setBrightness(0.3);

        driveKnob->randomizeEnabled = false;
        levelKnob->randomizeEnabled = false;

        initialize();
    }

    void initialize()
    {
        using namespace Sapphire;

        isPowerGateActive = true;
        isQuiet = false;
        outputVerifyCounter = 0;
        slewer.enable(true);
        params[POWER_TOGGLE_PARAM].setValue(1.0f);

        // Set the defaults for how to interpret the slider values.
        // Determined experimentally to produce useful ranges.

        frictionMap = SliderMapping(SliderScale::Exponential, {1.3f, -4.5f});
        stiffnessMap = SliderMapping(SliderScale::Exponential, {-0.1f, 3.4f});
        spanMap = SliderMapping(SliderScale::Linear, {0.0008, 0.0003});
        curlMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});
        massMap = SliderMapping(SliderScale::Exponential, {0.0f, 1.0f});
        tiltMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});

        mp = CreateHex(mesh);
        INFO("Mesh has %d balls, %d springs.", mesh.NumBalls(), mesh.NumSprings());

        // Define how stereo inputs go into the mesh.
        leftInput  = MeshInput(mp.leftInputBallIndex);
        rightInput = MeshInput(mp.rightInputBallIndex);

        // Define how to extract stereo outputs from the mesh.
        leftOutput  = MeshOutput(mp.leftOutputBallIndex);
        rightOutput = MeshOutput(mp.rightOutputBallIndex);

        leftLoCut.Reset();
        leftLoCut.SetCutoffFrequency(dcRejectQuantity->frequency);
        rightLoCut.Reset();
        rightLoCut.SetCutoffFrequency(dcRejectQuantity->frequency);
        dcRejectQuantity->changed = false;
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
            float cv = inputs[cvInputId].getVoltageSum();
            // When the attenuverter is set to 100%, and the cv is +5V, we want
            // to swing a slider that is all the way down (minSlider)
            // to act like it is all the way up (maxSlider).
            // Thus we allow the complete range of control for any CV whose
            // range is [-5, +5] volts.
            slider += attenu * (cv / 5.0) * (maxSlider - minSlider);
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

    void quiet()
    {
        mesh.Quiet();
        leftLoCut.Reset();
        rightLoCut.Reset();
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
            // If the gate input is connected, use the polyphonic sum
            // to control whether POWER is enabled or disabled.
            // Debounce the signal using hysteresis like a Schmitt trigger would.
            // See: https://vcvrack.com/manual/VoltageStandards#Triggers-and-Gates
            const float gv = gate.getVoltageSum();
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

            // If this is the first sample since Elastika was turned off,
            // force the mesh to go back to its starting state:
            // all balls back where they were, and cease all movement.
            if (!isQuiet)
            {
                isQuiet = true;
                quiet();
            }
            return;
        }

        isQuiet = false;

        // If the user has changed the DC cutoff via the right-click menu,
        // update the output filter corner frequencies.
        if (dcRejectQuantity->changed)
        {
            dcRejectQuantity->changed = false;
            leftLoCut.SetCutoffFrequency(dcRejectQuantity->frequency);
            rightLoCut.SetCutoffFrequency(dcRejectQuantity->frequency);
        }

        // Update the mesh parameters from sliders and control voltages.

        float halfLife = getControlValue(frictionMap, FRICTION_SLIDER_PARAM, FRICTION_ATTEN_PARAM, FRICTION_CV_INPUT);
        float restLength = getControlValue(spanMap, SPAN_SLIDER_PARAM, SPAN_ATTEN_PARAM, SPAN_CV_INPUT);
        float stiffness = getControlValue(stiffnessMap, STIFFNESS_SLIDER_PARAM, STIFFNESS_ATTEN_PARAM, STIFFNESS_CV_INPUT);
        float curl = getControlValue(curlMap, CURL_SLIDER_PARAM, CURL_ATTEN_PARAM, CURL_CV_INPUT, -1.0f, +1.0f);
        float mass = getControlValue(massMap, MASS_SLIDER_PARAM, MASS_ATTEN_PARAM, MASS_CV_INPUT, -1.0f, +1.0f);
        float drive = std::pow(params[DRIVE_KNOB_PARAM].getValue(), 4.0f);
        float gain = std::pow(params[LEVEL_KNOB_PARAM].getValue(), 4.0f);
        float inTilt = getControlValue(tiltMap, INPUT_TILT_KNOB_PARAM, INPUT_TILT_ATTEN_PARAM, INPUT_TILT_CV_INPUT);
        float outTilt = getControlValue(tiltMap, OUTPUT_TILT_KNOB_PARAM, OUTPUT_TILT_ATTEN_PARAM, OUTPUT_TILT_CV_INPUT);

        mesh.SetRestLength(restLength);
        mesh.SetStiffness(stiffness);

        Ball& lmBall = mesh.GetBallAt(mp.leftVarMassBallIndex);
        Ball& rmBall = mesh.GetBallAt(mp.rightVarMassBallIndex);
        lmBall.mass = rmBall.mass = 1.0e-6 * mass;

        if (curl >= 0.0f)
            mesh.SetMagneticField(curl * PhysicsVector(0.005, 0, 0, 0));
        else
            mesh.SetMagneticField(curl * PhysicsVector(0, 0, -0.005, 0));

        // Feed audio stimulus into the mesh.
        PhysicsVector leftInputDir = Interpolate(inTilt, mp.leftInputDir1, mp.leftInputDir2);
        PhysicsVector rightInputDir = Interpolate(inTilt, mp.rightInputDir1, mp.rightInputDir2);
        leftInput.Inject(mesh, leftInputDir, drive * inputs[AUDIO_LEFT_INPUT].getVoltageSum());
        rightInput.Inject(mesh, rightInputDir, drive * inputs[AUDIO_RIGHT_INPUT].getVoltageSum());

        // Update the simulation state by one sample's worth of time.
        mesh.Update(args.sampleTime, halfLife);

        float sample[2];

        // Extract output for the left channel.
        PhysicsVector leftOutputDir = Interpolate(outTilt, mp.leftOutputDir1, mp.leftOutputDir2);
        sample[0] = leftOutput.Extract(mesh, leftOutputDir);
        sample[0] = leftLoCut.UpdateHiPass(sample[0], args.sampleRate);
        sample[0] *= gain;

        // Extract output for the right channel.
        PhysicsVector rightOutputDir = Interpolate(outTilt, mp.rightOutputDir1, mp.rightOutputDir2);
        sample[1] = rightOutput.Extract(mesh, rightOutputDir);
        sample[1] = rightLoCut.UpdateHiPass(sample[1], args.sampleRate);
        sample[1] *= gain;

        // Filter the audio through the slewer to prevent clicks during power transitions.
        slewer.process(sample, 2);

        // Final line of defense against NAN/infinite output:
        // Check for invalid output. If found, clear the mesh.
        // Do this about every quarter of a second, to avoid CPU burden.
        // The intention is for the user to notice something sounds wrong,
        // the output is briefly NAN, but then it clears up as soon as the
        // internal or external problem is resolved.
        // The main point is to avoid leaving Elastika stuck in a NAN state forever.
        if (++outputVerifyCounter >= 11000)
        {
            outputVerifyCounter = 0;
            if (!std::isfinite(sample[0]) || !std::isfinite(sample[1]))
            {
                quiet();
                sample[0] = sample[1] = 0.0f;
            }
        }

        outputs[AUDIO_LEFT_OUTPUT].setVoltage(sample[0]);
        outputs[AUDIO_RIGHT_OUTPUT].setVoltage(sample[1]);
    }
};


struct ElastikaWidget : ModuleWidget
{
    Elastika *elastikaModule;

    ElastikaWidget(Elastika* module)
    {
        elastikaModule = module;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/elastika.svg")));

        // Sliders
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec( 8.00, 46.00)), module, Elastika::FRICTION_SLIDER_PARAM, Elastika::FRICTION_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(19.24, 46.00)), module, Elastika::STIFFNESS_SLIDER_PARAM, Elastika::STIFFNESS_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(30.48, 46.00)), module, Elastika::SPAN_SLIDER_PARAM, Elastika::SPAN_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(41.72, 46.00)), module, Elastika::CURL_SLIDER_PARAM, Elastika::CURL_LIGHT));
        addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(52.96, 46.00)), module, Elastika::MASS_SLIDER_PARAM, Elastika::MASS_LIGHT));

        // Attenuverters
        addParam(createParamCentered<Trimpot>(mm2px(Vec( 8.00, 72.00)), module, Elastika::FRICTION_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(19.24, 72.00)), module, Elastika::STIFFNESS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48, 72.00)), module, Elastika::SPAN_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(41.72, 72.00)), module, Elastika::CURL_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(52.96, 72.00)), module, Elastika::MASS_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec( 8.00, 12.50)), module, Elastika::INPUT_TILT_ATTEN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(53.00, 12.50)), module, Elastika::OUTPUT_TILT_ATTEN_PARAM));

        // Drive and Level knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(14.00, 102.00)), module, Elastika::DRIVE_KNOB_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(46.96, 102.00)), module, Elastika::LEVEL_KNOB_PARAM));

        // Tilt angle knobs
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(19.24, 17.50)), module, Elastika::INPUT_TILT_KNOB_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(41.72, 17.50)), module, Elastika::OUTPUT_TILT_KNOB_PARAM));

        // CV input jacks
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 8.00, 81.74)), module, Elastika::FRICTION_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(19.24, 81.74)), module, Elastika::STIFFNESS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(30.48, 81.74)), module, Elastika::SPAN_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(41.72, 81.74)), module, Elastika::CURL_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(52.96, 81.74)), module, Elastika::MASS_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec( 8.00, 22.50)), module, Elastika::INPUT_TILT_CV_INPUT));
        addInput(createInputCentered<SapphirePort>(mm2px(Vec(53.00, 22.50)), module, Elastika::OUTPUT_TILT_CV_INPUT));

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

    void appendContextMenu(Menu* menu) override
    {
        if (elastikaModule && elastikaModule->dcRejectQuantity)
        {
            menu->addChild(new MenuSeparator);
            DcRejectSlider *dcRejectSlider = new DcRejectSlider(elastikaModule->dcRejectQuantity);
            dcRejectSlider->box.size.x = 200.0f;
            menu->addChild(dcRejectSlider);
        }
    }
};


Model* modelElastika = createModel<Elastika, ElastikaWidget>("Elastika");
