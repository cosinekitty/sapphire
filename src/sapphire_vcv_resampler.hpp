// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire
#pragma once
#include "plugin.hpp"

namespace Sapphire
{
    namespace Resampler
    {
        template <unsigned maxChannels>
        struct Frame
        {
            float sample[maxChannels]{};

            void clear()
            {
                for (unsigned i = 0; i < maxChannels; ++i)
                    sample[i] = 0;
            }
        };


        template <unsigned maxChannels, unsigned maxQueueFrames>
        struct ShiftQueue
        {
            Frame<maxChannels> buffer[maxQueueFrames];
            unsigned length = 0;

            void append(const Frame<maxChannels>& frame)
            {
                if (length < maxQueueFrames)
                    buffer[length++] = frame;
            }

            void consume(unsigned nframes)
            {
                if (nframes > length)
                {
                    length = 0;
                }
                else
                {
                    for (unsigned i = 0; i + nframes < length; ++i)
                        buffer[i] = buffer[i + nframes];

                    length -= nframes;
                }
            }

            unsigned stride() const
            {
                return maxChannels;
            }

            float* front()
            {
                return &buffer[0].sample[0];
            }

            float* back()
            {
                return &buffer[length].sample[0];
            }

            bool empty() const
            {
                return length == 0;
            }

            bool full() const
            {
                return length == maxQueueFrames;
            }

            unsigned unusedFrameCount() const
            {
                return maxQueueFrames - length;
            }
        };


        template <unsigned maxInputChannels, unsigned maxOutputChannels>
        class InternalModel
        {
        public:
            virtual void processInternalFrame(
                const Frame<maxInputChannels>& inFrame,
                Frame<maxOutputChannels>& outFrame) = 0;
        };


        template <unsigned maxInputChannels, unsigned maxOutputChannels, unsigned maxQueueFrames>
        struct Hamburger
        {
            // I call this a Hamburger because it sandwiches an internal "model" sample rate
            // between two common external "signal" sample rates.
            // For example, a process() method can be operating at 44.1 kHz signal rate,
            // with a physical model operating at a rate of 48 kHz.
            // Input arrives, and output leaves, at 44.1 kHz, while the internal model operates
            // at 48 kHz. This pattern ensures consistent behavior of physical models
            // for a variable signal rate.

            ShiftQueue<maxInputChannels, maxQueueFrames> signalInQueue;
            dsp::SampleRateConverter<maxInputChannels> inResamp;
            ShiftQueue<maxInputChannels, maxQueueFrames> modelInQueue;

            ShiftQueue<maxOutputChannels, maxQueueFrames> modelOutQueue;
            dsp::SampleRateConverter<maxOutputChannels> outResamp;
            ShiftQueue<maxOutputChannels, maxQueueFrames> signalOutQueue;

            void setRates(int signalSampleRate, int modelSampleRate)
            {
                inResamp.setRates(signalSampleRate, modelSampleRate);
                outResamp.setRates(modelSampleRate, signalSampleRate);
            }

            void process(
                InternalModel<maxInputChannels, maxOutputChannels>& model,
                const Frame<maxInputChannels>& signalInFrame,
                unsigned inputChannelCount,
                Frame<maxOutputChannels>& signalOutFrame,
                unsigned outputChannelCount)
            {
                // Enqueue in the inbound signal frame.
                signalInQueue.append(signalInFrame);

                // Feed as many inbound signal frames as possible through the input resampler.
                int signalInFrameCount = signalInQueue.length;
                int modelInFrameCount = modelInQueue.unusedFrameCount();

                inResamp.setChannels(inputChannelCount);

                inResamp.process(
                    signalInQueue.front(),
                    maxInputChannels,
                    &signalInFrameCount,
                    modelInQueue.back(),
                    maxInputChannels,
                    &modelInFrameCount
                );
                signalInQueue.consume(signalInFrameCount);

                // Run as many frames as possible through the model.

                Frame<maxOutputChannels> modelOutFrame;
                unsigned eatCount = 0;
                for (unsigned f = 0; f < modelInQueue.length && !modelOutQueue.full(); ++f)
                {
                    model.processInternalFrame(modelInQueue.buffer[f], modelOutFrame);
                    modelOutQueue.append(modelOutFrame);
                    ++eatCount;
                }
                modelInQueue.consume(eatCount);

                // Resample as many model output frames as possible.

                int modelOutFrameCount = modelOutQueue.length;
                int signalOutFrameCount = signalOutQueue.unusedFrameCount();

                outResamp.setChannels(outputChannelCount);

                outResamp.process(
                    modelOutQueue.front(),
                    maxOutputChannels,
                    &modelOutFrameCount,
                    signalOutQueue.back(),
                    maxOutputChannels,
                    &signalOutFrameCount
                );

                // Hopefully we have at least one output frame available,
                // but we might not at the very beginning. When that happens,
                // output a frame full of zeroes.
                if (signalOutQueue.empty())
                {
                    signalOutFrame.clear();
                }
                else
                {
                    signalOutFrame = signalOutQueue.buffer[0];
                    signalOutQueue.consume(1);
                }
            }
        };
    }
}
