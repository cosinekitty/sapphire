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
        box.size.x = 200.0f;
    }
};


struct VoltageQuantity : SapphireQuantity
{
    std::string getDisplayValueString() override
    {
        return string::f("%0.3f", value);
    }
};


struct VoltageSlider : ui::Slider
{
    VoltageSlider(VoltageQuantity *_quantity)
    {
        quantity = _quantity;
        box.size.x = 200.0f;
    }
};
