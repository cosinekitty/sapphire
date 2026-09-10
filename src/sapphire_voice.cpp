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
        updatePhase(sampleRateHz, context.freq);

        // FIXFIXFIX : Here we calculate a fixed 90° phase angle between left and right.
        // FIXFIXFIX : Consider defaulting to 0° with ±180° adjustment via one of the MOD controls.
        // One interesting idea would be push/pull where half the adjustment is added to the right
        // channel, and half is subtracted from the left channel. That way, audio rate modulation
        // applies equally to both channels.

        static constexpr float twopi = 2*M_PI;
        const float radians = twopi * phase;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return StereoFrame(env*c, env*s);
    }

    //--------------------------------------------------------------------------------------------------

    void SawEngine::initialize()
    {
        phase = 0;
        envelope.initialize();
    }


    StereoFrame SawEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        const float env = PEAK_VOLTS * envelope.process(sampleRateHz, context);
        updatePhase(sampleRateHz, context.freq);
        const float bipolar = 2*phase - 1;
        // FIXFIXFIX - add rack::dsp::MinBlepGenerator correction for anti-aliasing.
        return StereoFrame(env*bipolar, env*bipolar);
    }

    //--------------------------------------------------------------------------------------------------

    void TriangleEngine::initialize()
    {
        phase = 0;
        envelope.initialize();
    }


    StereoFrame TriangleEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        const float env = PEAK_VOLTS * envelope.process(sampleRateHz, context);
        updatePhase(sampleRateHz, context.freq);

        float fraction;

        if (phase < 0.25)
        {
            // Rise from 0 to 1   :  0.00 <= phase < 0.25
            fraction = 4*phase;
        }
        else if (phase < 0.75)
        {
            // Sink from 1 to -1  :  0.25 <= phase < 0.75
            fraction = 2 - 4*phase;
        }
        else
        {
            // Rise from -1 to 0  :  0.75 <= phase < 1.00
            fraction = 4*phase - 4;
        }

        // FIXFIXFIX - add rack::dsp::MinBlepGenerator correction for anti-aliasing.

        return StereoFrame(env*fraction, env*fraction);
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