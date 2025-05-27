#pragma once
#include <map>
#include <string>

namespace Sapphire
{
    struct ComponentLocation
    {
        float cx;     // x-coordinate of control's center
        float cy;     // y-coordinate of control's center

        ComponentLocation(float _cx, float _cy)
            : cx(_cx)
            , cy(_cy)
            {}
    };


    ComponentLocation FindComponent(
        const std::string& moduleCode,      // symbol for the module, e.g. "elastika"
        const std::string& label);          // symbol for the component, e.g. "mix_knob"


    inline ComponentLocation GetPanelDimensions(const std::string& moduleCode)
    {
        return FindComponent(moduleCode, "_panel");
    }

    inline float PanelWidth(const std::string& moduleCode)
    {
        return GetPanelDimensions(moduleCode).cx;
    }

    inline float PanelHeight(const std::string& moduleCode)
    {
        return GetPanelDimensions(moduleCode).cy;
    }
}
