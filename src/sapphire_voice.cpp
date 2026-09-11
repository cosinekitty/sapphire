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

    void VoiceEngine::blepSquare(float sampleRateHz, const VoiceContext &context)
    {
        // IMPORTANT: This function is used directly for square waves,
        // and integrated with respect to time for triangle waves.
        //
        // The square wave has an adjustable duty cycle.
        //
        //      0 <= phase < duty   -->   voltage = +5V
        //      duty <= phase < 1   -->   voltage = -5V
        //
        // There are 2 discontinuities in the above function:
        // 1. when the low cycle ends at phase=0.       +10V jump
        // 2. when the duty cycle ends at phase=duty.   -10V drop

        const float delta = context.freq / sampleRateHz;    // cycles/sample

        phase += delta;
        if (phase >= 1)
        {
            phase -= 1;
            blep.insertDiscontinuity(-phase/delta, +2);
        }

        if (phase < context.duty)
        {
            square = +1;
            prevState = true;
        }
        else
        {
            if (prevState)
                blep.insertDiscontinuity((context.duty - phase)/delta, -2);

            square = -1;
            prevState = false;
        }

        square += blep.process();
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
        phase = FMOD<float>(phase + (context.freq / sampleRateHz), 1);

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
        const float delta = context.freq / sampleRateHz;    // cycles/sample

        phase += delta;
        if (phase >= 1)
        {
            phase -= 1;
            blep.insertDiscontinuity(-phase/delta, -2);
        }

        const float bipolar = 2*phase - 1;
        const float correction = blep.process();
        const float signal = env * (bipolar + correction);
        return StereoFrame(signal, signal);
    }

    //--------------------------------------------------------------------------------------------------

    void TriangleEngine::initialize()
    {
        phase = 0;
        square = triangle = 0;
        prevState = false;
        envelope.initialize();
    }


    StereoFrame TriangleEngine::process(float sampleRateHz, const VoiceContext &context)
    {
        const float env = PEAK_VOLTS * envelope.process(sampleRateHz, context);
        const float delta = context.freq / sampleRateHz;    // cycles/sample

        blepSquare(sampleRateHz, context);

        // Now the square wave signal is up to date.
        // Integrate it to obtain the triangle wave signal.
        triangle += 4 * delta * square;     // need slope=4 for ±1 peak amplitudes.

        // Apply a super-simple DC blocker to prevent DC drift in the output.
        triangle -= 0.0001f * triangle;     // gently push toward zero

        return StereoFrame(env*triangle, env*triangle);
    }

    //--------------------------------------------------------------------------------------------------

    void SquareEngine::initialize()
    {
        phase = 0;
        square = 0;
        envelope.initialize();
        prevState = false;
    }


    StereoFrame SquareEngine::process(float sampleRateHz, const VoiceContext& context)
    {
        const float env = PEAK_VOLTS * envelope.process(sampleRateHz, context);
        blepSquare(sampleRateHz, context);
        return StereoFrame(env*square, env*square);
    }

    //--------------------------------------------------------------------------------------------------
}