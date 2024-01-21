#pragma once
#include <cstddef>
#include <cstring>
#include <cinttypes>

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

            // Add any future fields here. Do not change anything above this line.
            // Update the version number as needed in the constructor call below.

            Message()
                : header(1, sizeof(Message))
                {}
        };

        inline bool IsValidMessage(const Message *message)
        {
            return
                (message != nullptr) &&
                (message->header.size >= sizeof(Message)) &&
                (0 == memcmp(message->header.signature, "Tcdr", 4)) &&
                (message->header.version >= 1);
        }

        struct Communicator     // allows two-way message traffic between Tricorder and an adjacent module
        {
            Message buffer[2];
            Module& parentModule;

            explicit Communicator(Module& module)
                : parentModule(module)
            {
                module.rightExpander.producerMessage = &buffer[0];
                module.rightExpander.consumerMessage = &buffer[1];
            }

            void sendVector(float x, float y, float z)
            {
                Tricorder::Message& msg = *static_cast<Tricorder::Message*>(parentModule.rightExpander.producerMessage);
                msg.x = x;
                msg.y = y;
                msg.z = z;
                parentModule.rightExpander.requestMessageFlip();
            }
        };
    }
}
