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
                if (gate)
                {
                    // While the note is held, we want it to fade from fraction=1 to fraction=sustain.
                    if (context.sustain >= 1)
                    {
                        // hold at current power forever - leave fraction alone
                    }
                    else
                    {
                        double descend = 1 - context.sustain;   // how far down we need to go
                        double decaySamples = sampleRateHz * rampTimeSeconds(context.decay / descend);
                        fraction = std::clamp<double>(fraction - 1/decaySamples, context.sustain, 1);
                    }
                }
                else
                {
                    state = AdsrState::Release;
                }
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

    //--------------------------------------------------------------------------------------------------

    void SineEngine::initialize()
    {
        phase = 0;
        envelope.initialize();
    }


    StereoFrame SineEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        const float env = PEAK_VOLTS * envelope.process(sampleRateHz, context);

        static constexpr float twopi = 2 * M_PI;

        phase = FMOD(
            phase + twopi*(context.freq / sampleRateHz),
            twopi
        );

        const float c = std::cos(phase);
        const float s = std::sin(phase);
        return StereoFrame(env*c, env*s);
    }

    //--------------------------------------------------------------------------------------------------

    void SawEngine::initialize()
    {
    }


    StereoFrame SawEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        return StereoFrame();
    }

    //--------------------------------------------------------------------------------------------------

    void TriangleEngine::initialize()
    {
    }


    StereoFrame TriangleEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        return StereoFrame();
    }

    //--------------------------------------------------------------------------------------------------

    void SquareEngine::initialize()
    {
    }


    StereoFrame SquareEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        return StereoFrame();
    }

    //--------------------------------------------------------------------------------------------------
}