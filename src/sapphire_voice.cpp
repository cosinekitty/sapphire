#include "sapphire_voice.hpp"

namespace Sapphire
{
    double AdsrEnvelope::process(float sampleRateHz, const VoiceContext &context)
    {
        const bool gate = context.gateTriggerReceiver.isGateActive();

        switch (state)
        {
            case AdsrState::Quiet:
            default:    // treat any invalid states as identical to Quiet
            {
                // On the rising edge of a gate, begin the attack phase.
                if (gate)
                {
                    state = AdsrState::Attack;
                    fraction = 0;
                }
            }
            break;

            case AdsrState::Attack:
            {
                // Gradually rise from 0 to 1.
                double rampSamples = sampleRateHz * rampTimeSeconds(context.attack);
                fraction = std::clamp<double>(fraction + 1/rampSamples, 0, 1);

                if (gate)
                {
                    if (fraction == 1)
                        state = AdsrState::Decay;
                }
                else
                {
                    state = AdsrState::Release;
                }
            }
            break;

            case AdsrState::Decay:
            {
                // Keep envelope at full power until the gate goes away.
                // Later we will use the sustain control that fades to a DECAY percentage.
                if (!gate)
                    state = AdsrState::Release;
            }
            break;

            case AdsrState::Release:
            {
                double rampSamples = sampleRateHz * rampTimeSeconds(context.release);
                fraction = std::clamp<double>(fraction - 1/rampSamples, 0, 1);
                if (gate)
                {
                    state = AdsrState::Attack;
                }
                else
                {
                    if (fraction == 0)
                        state = AdsrState::Quiet;
                }
            }
            break;
        }

        return fraction;
    }



    void SineEngine::initialize()
    {
        phase = 0;
        envelope.initialize();
    }


    StereoFrame SineEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        const float env = PEAK_VOLTS * envelope.process(sampleRateHz, context);

        static constexpr float twopi = 2 * M_PI;

        phase += twopi * (context.freq / sampleRateHz);
        if (phase <= -twopi)
            phase += twopi;
        if (phase >= +twopi)
            phase -= twopi;

        const float c = std::cos(phase);
        const float s = std::sin(phase);
        return StereoFrame(env*c, env*s);
    }


    void SawEngine::initialize()
    {
    }


    StereoFrame SawEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        return StereoFrame();
    }


    void TriangleEngine::initialize()
    {
    }


    StereoFrame TriangleEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        return StereoFrame();
    }


    void SquareEngine::initialize()
    {
    }


    StereoFrame SquareEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        return StereoFrame();
    }
}