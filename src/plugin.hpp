#pragma once
#include <rack.hpp>

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelMoots;
extern Model* modelElastika;
extern Model* modelTubeUnit;

struct SapphireControlGroup     // represents the combination: knob + CV input + attenuverter
{
    std::string name;
    int paramId;
    int attenId;
    int inputId;
    float minValue;
    float maxValue;
    float defaultValue;
    std::string unit;
    float displayBase;
    float displayMultiplier;

    SapphireControlGroup(
        std::string _name,
        int _paramId,
        int _attenId,
        int _inputId,
        float _minValue,
        float _maxValue,
        float _defaultValue,
        std::string _unit = "",
        float _displayBase = 0.0f,
        float _displayMultiplier = 1.0f
    )
        : name(_name)
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
    float value;
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
    DcRejectSlider(DcRejectQuantity *_quantity)
    {
        quantity = _quantity;
        box.size.x = 200.0f;        // without this, the menu display gets messed up
    }
};


const float AGC_LEVEL_MIN = 5.0f;
const float AGC_LEVEL_DEFAULT = 8.5f;
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
    AgcLevelSlider(AgcLevelQuantity *_quantity)
    {
        quantity = _quantity;
        box.size.x = 200.0f;
    }
};
