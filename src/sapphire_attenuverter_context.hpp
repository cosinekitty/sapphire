#pragma once
namespace Sapphire
{
    constexpr float AttenuverterLowSensitivityDenom = 10;

    struct SapphireAttenuverterContext
    {
        bool lowSensitivityMode{};
        bool unipolar{};    // replace any negative CV input with 0, whether from cables or internal chaos generators
    };
}