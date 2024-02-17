#pragma once
#include <cstddef>
#include <cstring>
#include <cinttypes>
#include "plugin.hpp"

namespace Sapphire
{
    namespace Tricorder
    {
        struct MessageHeader
        {
            std::size_t size;
            char signature[4];
            std::uint32_t version;

            MessageHeader(std::uint32_t _version, std::size_t _size)
                : size(_size)
                , version(_version)
            {
                std::memcpy(signature, "Tcdr", 4);
            }
        };

        struct Message
        {
            const MessageHeader header;
            float x{};
            float y{};
            float z{};
            char flag{};

            // Add any future fields here. Do not change anything above this line.
            // Update the version number as needed in the constructor call below.

            Message()
                : header(2, sizeof(Message))
                {}

            void setVector(float _x, float _y, float _z, bool reset)
            {
                x = _x;
                y = _y;
                z = _z;
                flag = reset ? 'V' : 'v';
            }

            bool isResetRequested() const
            {
                return flag == 'V';
            }
        };

        inline bool IsValidMessage(const Message *message)
        {
            return
                (message != nullptr) &&
                (message->header.size >= sizeof(Message)) &&
                (0 == memcmp(message->header.signature, "Tcdr", 4)) &&
                (message->header.version >= 2);
        }

        inline bool IsVectorMessage(const Message *message)
        {
            return IsValidMessage(message) && (message->flag == 'v' || message->flag == 'V');
        }

        struct VectorSender     // allows another module to send vectors into Tricorder/Tout from the left side
        {
            Message buffer[2];
            Module& parentModule;

            static bool isVectorReceiver(const Module* module)
            {
                return (module != nullptr) && ModelInfo::canReceiveVectors(module->model);
            }

            explicit VectorSender(Module& module)
                : parentModule(module)
            {
                module.rightExpander.producerMessage = &buffer[0];
                module.rightExpander.consumerMessage = &buffer[1];
            }

            void sendVector(float x, float y, float z, bool reset)
            {
                Tricorder::Message& prod = *static_cast<Tricorder::Message*>(parentModule.rightExpander.producerMessage);
                prod.setVector(x, y, z, reset);
                parentModule.rightExpander.requestMessageFlip();
            }

            bool isVectorReceiverConnectedOnRight() const
            {
                return isVectorReceiver(parentModule.rightExpander.module);
            }
        };


        struct VectorReceiver   // shared code for Tricorder and Tout to receive vectors from a module on the left
        {
            Message buffer[2];
            Module& parentModule;

            static bool isVectorSender(const Module* module)
            {
                return (module != nullptr) && ModelInfo::canSendVectors(module->model);
            }

            explicit VectorReceiver(Module& module)
                : parentModule(module)
            {
                module.leftExpander.producerMessage = &buffer[0];
                module.leftExpander.consumerMessage = &buffer[1];
            }

            bool isVectorSenderConnectedOnLeft() const
            {
                return isVectorSender(parentModule.leftExpander.module);
            }

            Message* inboundVectorMessage() const
            {
                const Module* lm = parentModule.leftExpander.module;
                if (isVectorSender(lm))
                {
                    Message* message = static_cast<Message *>(lm->rightExpander.consumerMessage);
                    if (IsVectorMessage(message))
                        return message;
                }
                return nullptr;
            }
        };
    }
}
