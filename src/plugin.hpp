#pragma once
#include <rack.hpp>
#include <vector>

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelElastika;
extern Model* modelFrolic;
extern Model* modelGlee;
extern Model* modelHiss;
extern Model* modelMoots;
extern Model* modelNucleus;
extern Model* modelPolynucleus;
extern Model* modelTin;
extern Model* modelTout;
extern Model* modelTricorder;
extern Model* modelTubeUnit;

namespace Sapphire
{
    struct ControlGroup     // represents the combination: knob + CV input + attenuverter
    {
        std::string name;
        int yGrid;
        int xGrid;
        int paramId;
        int attenId;
        int inputId;
        float minValue;
        float maxValue;
        float defaultValue;
        std::string unit;
        float displayBase;
        float displayMultiplier;

        ControlGroup(
            const std::string& _name,
            int _yGrid,
            int _xGrid,
            int _paramId,
            int _attenId,
            int _inputId,
            float _minValue,
            float _maxValue,
            float _defaultValue,
            const std::string& _unit = "",
            float _displayBase = 0.0f,
            float _displayMultiplier = 1.0f
        )
            : name(_name)
            , yGrid(_yGrid)
            , xGrid(_xGrid)
            , paramId(_paramId)
            , attenId(_attenId)
            , inputId(_inputId)
            , minValue(_minValue)
            , maxValue(_maxValue)
            , defaultValue(_defaultValue)
            , unit(_unit)
            , displayBase(_displayBase)
            , displayMultiplier(_displayMultiplier)
        {}
    };

    // Custom controls for Sapphire modules.

    struct SapphirePort : app::SvgPort
    {
        SapphirePort()
        {
            setSvg(Svg::load(asset::plugin(pluginInstance, "res/port.svg")));
        }
    };


    struct SapphireQuantity : ParamQuantity
    {
        float value = 0.0f;
        bool changed = true;

        SapphireQuantity()
        {
            randomizeEnabled = false;
        }

        void setValue(float newValue) override
        {
            float clamped = math::clamp(newValue, getMinValue(), getMaxValue());
            if (clamped != value)
            {
                changed = true;
                value = clamped;
            }
        }

        float getValue() override { return value; }

        void setDisplayValue(float displayValue) override { setValue(displayValue); }

        void save(json_t* root, const char *name)
        {
            json_object_set_new(root, name, json_real(value));
        }

        void load(json_t* root, const char *name)
        {
            json_t *level = json_object_get(root, name);
            if (json_is_number(level))
            {
                double x = json_number_value(level);
                setValue(x);
            }
        }

        void initialize()
        {
            float x = getDefaultValue();
            setValue(x);
        }
    };


    const float DC_REJECT_MIN_FREQ = 20.0f;
    const float DC_REJECT_MAX_FREQ = 400.0f;
    const float DC_REJECT_DEFAULT_FREQ = 20.0f;


    struct DcRejectQuantity : SapphireQuantity
    {
        std::string getDisplayValueString() override
        {
            return string::f("%i", (int)(math::normalizeZero(value) + 0.5f));
        }
    };


    struct DcRejectSlider : ui::Slider
    {
        explicit DcRejectSlider(DcRejectQuantity *_quantity)
        {
            quantity = _quantity;
            box.size.x = 200.0f;        // without this, the menu display gets messed up
        }
    };


    const float AGC_LEVEL_MIN = 1.0f;
    const float AGC_LEVEL_DEFAULT = 4.0f;
    const float AGC_LEVEL_MAX = 10.0f;
    const float AGC_DISABLE_MIN = 10.1f;
    const float AGC_DISABLE_MAX = 10.2f;


    struct AgcLevelQuantity : SapphireQuantity
    {
        bool isAgcEnabled() const { return value < AGC_DISABLE_MIN; }
        float clampedAgc() const { return clamp(value, AGC_LEVEL_MIN, AGC_LEVEL_MAX); }

        std::string getDisplayValueString() override
        {
            return isAgcEnabled() ? string::f("%0.2f V", clampedAgc()) : "OFF";
        }
    };


    struct AgcLevelSlider : ui::Slider
    {
        explicit AgcLevelSlider(AgcLevelQuantity *_quantity)
        {
            quantity = _quantity;
            box.size.x = 200.0f;
        }
    };

    // My custom overlay class is very similar to SvgPanel,
    // only without a border drawn around it.
    // It provides a transparent overlay on top of my main SvgPanel,
    // and enables hiding/showing layers at will.
    struct SvgOverlay : Widget
    {
        FramebufferWidget* fb;
        SvgWidget *sw;
        std::shared_ptr<window::Svg> svg;

        explicit SvgOverlay(std::shared_ptr<window::Svg> _svg)
        {
            fb = new FramebufferWidget;
            addChild(fb);

            sw = new SvgWidget;
            fb->addChild(sw);

            svg = _svg;
            sw->setSvg(_svg);

            box.size = fb->box.size = sw->box.size;
        }

        static SvgOverlay* Load(std::string relativeFileName)
        {
            std::string filename = asset::plugin(pluginInstance, relativeFileName);
            std::shared_ptr<Svg> svg = Svg::load(filename);
            return new SvgOverlay(svg);
        }

        void step() override
        {
            fb->oversample = (APP->window->pixelRatio < 2.0f) ? 2.0f : 1.0f;
            Widget::step();
        }
    };


    class GateTriggerReceiver
    {
    private:
        float prevVoltage{};
        bool gate{};

    public:
        void initialize()
        {
            prevVoltage = 0;
            gate = false;
        }

        bool updateGate(float voltage)
        {
            if (prevVoltage < 1.0f && voltage >= 1.0f)
                gate = true;
            else if (prevVoltage >= 0.1f && voltage < 0.1f)
                gate = false;
            prevVoltage = voltage;
            return gate;
        }

        bool updateTrigger(float voltage)
        {
            bool prevGate = gate;
            updateGate(voltage);
            return gate && !prevGate;   // a trigger occurs on the rising edge of a gate
        }
    };


    class TriggerSender
    {
    private:
        const float duration = 0.001f;  // a trigger should last at least one millisecond
        float elapsed = 0;
        bool isFiring = false;

    public:
        void initialize()
        {
            elapsed = 0;
            isFiring = false;
        }

        float process(float dt, bool fire)
        {
            if (fire)
            {
                elapsed = 0;
                isFiring = true;
            }
            if (isFiring)
            {
                if (elapsed >= duration)
                    isFiring = false;
                elapsed += dt;
                return 10;
            }
            return 0;
        }
    };


    struct SapphireModule : public Module
    {
        float getControlValue(int paramId, int attenId, int inputId, float minValue = 0, float maxValue = 1)
        {
            float slider = params[paramId].getValue();
            float cv = inputs[inputId].getVoltageSum();
            // When the attenuverter is set to 100%, and the cv is +5V, we want
            // to swing a slider that is all the way down (minSlider)
            // to act like it is all the way up (maxSlider).
            // Thus we allow the complete range of control for any CV whose
            // range is [-5, +5] volts.
            float attenu = params[attenId].getValue();
            slider += attenu*(cv / 5)*(maxValue - minValue);
            return clamp(slider, minValue, maxValue);
        }
    };


    struct AutomaticLimiterModule : public SapphireModule   // a Sapphire module with a warning light on the OUTPUT knob
    {
        bool enableLimiterWarning = true;
        int recoveryCountdown = 0;      // positive integer when we make OUTPUT knob pink to indicate "NAN crash"

        virtual double getAgcDistortion() const = 0;

        virtual NVGcolor getWarningColor()
        {
            if (recoveryCountdown > 0)
            {
                // The module is recovering from non-finite (NAN/infinite) output.
                // Inflict an obnoxiously bright pink OUTPUT knob glow on the user!
                return nvgRGBA(0xff, 0x00, 0xff, 0xb0);
            }

            const double distortion = getAgcDistortion();
            if (!enableLimiterWarning || distortion <= 0.0)
                return nvgRGBA(0, 0, 0, 0);     // no warning light

            double decibels = 20.0 * std::log10(1.0 + distortion);
            double scale = clamp(decibels / 24.0);

            int red   = colorComponent(scale, 0x90, 0xff);
            int green = colorComponent(scale, 0x20, 0x50);
            int blue  = 0x00;
            int alpha = 0x70;

            return nvgRGBA(red, green, blue, alpha);
        }

        static int colorComponent(double scale, int lo, int hi)
        {
            return clamp(static_cast<int>(round(lo + scale*(hi-lo))), lo, hi);
        }
    };


    class WarningLightWidget : public LightWidget
    {
    private:
        AutomaticLimiterModule *alModule;

    public:
        explicit WarningLightWidget(AutomaticLimiterModule *_alModule)
            : alModule(_alModule)
        {
            borderColor = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't draw a circular border
            bgColor     = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't mess with the knob behind the light
        }

        void drawLayer(const DrawArgs& args, int layer) override
        {
            if (layer == 1)
                color = alModule ? alModule->getWarningColor() : nvgRGBA(0, 0, 0, 0);

            LightWidget::drawLayer(args, layer);
        }
    };

    enum class VectorRole
    {
        None,
        Sender,
        Receiver,
        SenderAndReceiver,
    };

    struct ModelInfo
    {
        static ModelInfo *front;
        ModelInfo *next = nullptr;
        rack::plugin::Model* model;
        VectorRole vectorRole;

        ModelInfo(rack::plugin::Model* _model, VectorRole _vectorRole)
            : model(_model)
            , vectorRole(_vectorRole)
        {
        }

        static void insert(Model *_model, VectorRole _vectorRole)
        {
            ModelInfo *info = new ModelInfo(_model, _vectorRole);
            info->next = front;
            front = info;
        }

        static ModelInfo* search(const Model *model)
        {
            if (model != nullptr)
                for (ModelInfo *info = front; info != nullptr; info = info->next)
                    if (info->model == model)
                        return info;

            return nullptr;
        }

        static bool canReceiveVectors(const Model* model)
        {
            ModelInfo* info = search(model);
            if (info == nullptr)
                return false;

            return (info->vectorRole == VectorRole::Receiver) || (info->vectorRole == VectorRole::SenderAndReceiver);
        }

        static bool canSendVectors(const Model* model)
        {
            ModelInfo* info = search(model);
            if (info == nullptr)
                return false;

            return (info->vectorRole == VectorRole::Sender) || (info->vectorRole == VectorRole::SenderAndReceiver);
        }
    };
}

// Keep this in the global namespace, not inside "Sapphire".
template <class TModule, class TModuleWidget>
rack::plugin::Model* createSapphireModel(std::string slug, Sapphire::VectorRole vectorRole)
{
    Model *model = rack::createModel<TModule, TModuleWidget>(slug);
    Sapphire::ModelInfo::insert(model, vectorRole);
    return model;
}
