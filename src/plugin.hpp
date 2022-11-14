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


struct DcRejectQuantity : ParamQuantity
{
    float frequency = 20.0f;
    bool changed = true;

    DcRejectQuantity()
    {
        randomizeEnabled = false;
    }

    void setValue(float value) override
    {
        float newFrequency = math::clamp(value, getMinValue(), getMaxValue());
        if (newFrequency != frequency)
        {
            changed = true;
            frequency = newFrequency;
        }
    }

    float getValue() override { return frequency; }

    std::string getDisplayValueString() override
    {
        return string::f("%i", (int)(math::normalizeZero(frequency) + 0.5f));
    }

    void setDisplayValue(float displayValue) override { setValue(displayValue); }
};


struct DcRejectSlider : ui::Slider
{
    DcRejectSlider(DcRejectQuantity *_quantity)
    {
        quantity = _quantity;
    }
};
