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

            void initialize()
            {
                length = 0;
            }

            void append(const Frame<maxChannels>& frame)
            {
                if (length < maxQueueFrames)
                    buffer[length++] = frame;
            }

            void consume(unsigned nframes)
            {
                if (nframes == 0)
                    return;

                if (nframes >= length)
                {
                    length = 0;
                    return;
                }

                for (unsigned i = 0; i + nframes < length; ++i)
                    buffer[i] = buffer[i + nframes];

                length -= nframes;
            }

            void advance(unsigned nframes)
            {
                length += nframes;
                assert(length <= maxQueueFrames);
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
                Frame<maxOutputChannels>& outFrame,
                int sampleRate) = 0;
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

            int signalRate = 0;
            int modelRate = 0;

            bool canSkipResampler() const
            {
                // If the model sample rate is zero, it is a flag that
                // the caller wants to disable the resampler logic and operate
                // the model at the same rate as the signal.
                // Also, if both rates match, it means no resampling is necessary.
                return modelRate == 0 || modelRate == signalRate;
            }

            ShiftQueue<maxInputChannels, maxQueueFrames> signalInQueue;
            dsp::SampleRateConverter<maxInputChannels> inResamp;
            ShiftQueue<maxInputChannels, maxQueueFrames> modelInQueue;

            ShiftQueue<maxOutputChannels, maxQueueFrames> modelOutQueue;
            dsp::SampleRateConverter<maxOutputChannels> outResamp;
            ShiftQueue<maxOutputChannels, maxQueueFrames> signalOutQueue;

            void setRates(int signalSampleRate, int modelSampleRate)
            {
                signalRate = signalSampleRate;
                modelRate = modelSampleRate;
                if (!canSkipResampler())
                {
                    inResamp.setRates(signalRate, modelRate);
                    outResamp.setRates(modelRate, signalRate);
                }
            }

            void initialize()
            {
                signalInQueue.initialize();
                modelInQueue.initialize();
                modelOutQueue.initialize();
                signalOutQueue.initialize();
            }

            void process(
                InternalModel<maxInputChannels, maxOutputChannels>& model,
                const Frame<maxInputChannels>& signalInFrame,
                unsigned inputChannelCount,
                Frame<maxOutputChannels>& signalOutFrame,
                unsigned outputChannelCount)
            {
                if (canSkipResampler())
                {
                    model.processInternalFrame(signalInFrame, signalOutFrame, signalRate);
                    return;
                }

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
                modelInQueue.advance(modelInFrameCount);
                signalInQueue.consume(signalInFrameCount);

                // Run as many frames as possible through the model.

                Frame<maxOutputChannels> modelOutFrame;
                unsigned eatCount = 0;
                for (unsigned f = 0; f < modelInQueue.length && !modelOutQueue.full(); ++f)
                {
                    model.processInternalFrame(modelInQueue.buffer[f], modelOutFrame, modelRate);
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
                signalOutQueue.advance(signalOutFrameCount);
                modelOutQueue.consume(modelOutFrameCount);

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


        inline std::string ModelRateText(int rate)
        {
            return (rate > 0) ? (std::to_string(rate) + " Hz") : "Match engine rate";
        }


        struct ChangeModelSampleRateAction : history::Action
        {
            std::size_t& selectedIndex;
            const std::size_t oldValue;
            const std::size_t newValue;

            explicit ChangeModelSampleRateAction(std::size_t& _selectedIndex, int index, int rate)
                : selectedIndex(_selectedIndex)
                , oldValue(_selectedIndex)
                , newValue(index)
            {
                name = "change model sample rate to " + ModelRateText(rate);
            }

            void undo() override
            {
                selectedIndex = oldValue;
            }

            void redo() override
            {
                selectedIndex = newValue;
            }
        };


        struct ModelSampleRateChooser
        {
            std::size_t selectedIndex = 0;
            const std::vector<int> sampleRateOptions;

            explicit ModelSampleRateChooser(std::initializer_list<int> options)
                : sampleRateOptions(options)
                {}

            void initialize()
            {
                selectedIndex = 0;
            }

            void loadOrRevertToDefault(int& sampleRate)
            {
                // Search for an exact match.
                std::size_t index = 0;
                for (int rate : sampleRateOptions)
                {
                    if (sampleRate == rate)
                    {
                        selectedIndex = index;
                        return;
                    }
                    ++index;
                }

                // If not found, revert to default (index = 0).
                sampleRate = sampleRateOptions.at(0);
                selectedIndex = 0;
            }

            int getSelectedSampleRate() const
            {
                return sampleRateOptions.at(selectedIndex);
            }

            void addOptionsToMenu(Menu* menu)
            {
                std::vector<std::string> labels;
                for (int rate : sampleRateOptions)
                    labels.push_back(ModelRateText(rate));

                menu->addChild(createIndexSubmenuItem(
                    "Model sample rate",
                    labels,
                    [=]() { return selectedIndex; },
                    [=](std::size_t index)
                    {
                        if (index != selectedIndex)
                            InvokeAction(new ChangeModelSampleRateAction(selectedIndex, index, sampleRateOptions.at(index)));
                    }
                ));
            }
        };
    }
}
