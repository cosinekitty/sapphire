#include "sapphire_voice.hpp"

namespace Sapphire
{
    void SineEngine::initialize()
    {
        phase = 0;
    }

    StereoFrame SineEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        if (!context.gateTriggerReceiver.isGateActive())
            return StereoFrame();

        static constexpr float twopi = 2 * M_PI;

        phase += twopi * (context.freq / sampleRateHz);
        if (phase <= -twopi)
            phase += twopi;
        if (phase >= +twopi)
            phase -= twopi;

        const float c = std::cos(phase);
        const float s = std::sin(phase);
        static constexpr float amplitude = 0.95;
        return StereoFrame(amplitude*c, amplitude*s);
    }
}