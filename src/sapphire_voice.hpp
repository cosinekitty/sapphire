#pragma once
#include <array>
#include <algorithm>

namespace Sapphire
{
    constexpr unsigned NSTEREO = 2;
    constexpr unsigned NPOLY = 16;

    struct StereoFrame
    {
        std::array<float, NSTEREO> sample{};
    };


    struct VoiceEngine
    {
        virtual void initialize() = 0;
        virtual StereoFrame process(float sampleRateHz) = 0;
    };


    struct SineEngine : VoiceEngine
    {
        void initialize() override;
        StereoFrame process(float sampleRateHz) override;
    };


    struct PolyStereoFrame
    {
        unsigned nchannels{};
        std::array<StereoFrame, NPOLY> poly{};
    };


    struct PolyVoiceEngineBase
    {
        virtual void initialize() = 0;
        virtual PolyStereoFrame process(float sampleRateHz, unsigned nchannels) = 0;
    };


    template <typename engine_t>
    struct PolyVoiceEngine : PolyVoiceEngineBase
    {
        std::array<engine_t, NPOLY> engineArray;

        void initialize() override
        {
            for (engine_t& engine : engineArray)
                engine.initialize();
        }

        PolyStereoFrame process(float sampleRateHz, unsigned nchannels) override
        {
            PolyStereoFrame poly;
            poly.nchannels = std::clamp<unsigned>(nchannels, 0, NPOLY);
            for (unsigned c = 0; c < poly.nchannels; ++c)
                poly.poly[c] = engineArray[c].process(sampleRateHz);
            return poly;
        }
    };
}