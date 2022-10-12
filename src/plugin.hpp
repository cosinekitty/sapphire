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


class Slewer
{
private:
    enum SlewState
    {
        Disabled,       // there is no audio slewing: treat inputs as control voltages
        Off,            // audio slewing is enabled, but currently the output is disconnected
        Ramping,        // either a rising or falling linear ramp transitioning between connect/disconnect
        On,             // audio slewing is enabled, and currently the output is connected
    };

    SlewState state;
    int rampLength;
    int count;          // IMPORTANT: valid only when state == Ramping; must ignore otherwise

public:
    Slewer()
        : state(Disabled)
        , rampLength(1)
        , count(0)
        {}

    void setRampLength(int newRampLength)
    {
        rampLength = std::max(1, newRampLength);
    }

    void reset()
    {
        // Leave the rampLength alone. It should only be changed by changes in the sample rate.
        state = Disabled;
    }

    void enable(bool active)
    {
        state = active ? On : Off;
    }

    bool isEnabled() const
    {
        return state != Disabled;
    }

    bool update(bool active)
    {
        switch (state)
        {
        case Disabled:
            return active;

        case Off:
            if (active)
            {
                // Start an upward ramp.
                state = Ramping;
                count = 0;
            }
            break;

        case On:
            if (!active)
            {
                // Start a downward ramp.
                state = Ramping;
                count = rampLength - 1;
            }
            break;

        case Ramping:
            // Allow zig-zagging of the linear ramp if `active` keeps changing.
            if (active)
            {
                // Ramp upward.
                if (count < rampLength)
                    ++count;
                else
                    state = On;
            }
            else
            {
                // Ramp downward.
                if (count > 0)
                    --count;
                else
                    state = Off;
            }
            break;

        default:
            assert(false);      // invalid state -- should never happen
            break;
        }

        return state != Off;
    }

    void process(float volts[], int channels)
    {
        assert(channels >= 0 && channels <= PORT_MAX_CHANNELS);

        if (state != Ramping)
            return;     // not ramping, so we must ignore `count`

        if (channels < 1)
            return;     // another short-cut to save processing time

        // The sample rate could change at any moment,
        // including while we are ramping.
        // Therefore we need to make sure the ratio count/rampLength
        // is bounded to the range [0, 1].

        float gain = clamp(static_cast<float>(count) / static_cast<float>(rampLength));

        for (int c = 0; c < channels; ++c)
            volts[c] *= gain;
    }
};
