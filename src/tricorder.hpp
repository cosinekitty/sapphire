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

            void setReceipt()
            {
                flag = 'r';
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

        inline bool IsReturnReceiptMessage(const Message *message)
        {
            return IsValidMessage(message) && (message->flag == 'r');
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

            bool sendVector(float x, float y, float z, bool reset)
            {
                Tricorder::Message& prod = *static_cast<Tricorder::Message*>(parentModule.rightExpander.producerMessage);
                prod.setVector(x, y, z, reset);

                parentModule.rightExpander.requestMessageFlip();

                Tricorder::Message* cons = static_cast<Tricorder::Message*>(parentModule.rightExpander.consumerMessage);
                return IsReturnReceiptMessage(cons);    // did our previous message (if any) receive a return receipt?
            }
        };
    }
}
