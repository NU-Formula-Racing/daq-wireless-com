#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>

#define MSG_IDENTIFIER "NFR"
#define BIT_FLAG(x) (1 << x)
#define MAX_PACKET_SIZE 256 // in bytes

namespace wircom
{

    enum MessageType
    {
        MSG_REQUEST = 0,  // request message
        MSG_RESPONSE = 1, // response message
        MSG_ERROR = 2,    // error message
    };

    enum MessageContentType
    {
        MSG_CON_META = 0,             // meta data for drive
        MSG_CON_DRIVE = 1,            // .drive file
        MSG_CON_SWITCH_DATA_RATE = 2, // switch data rate
        MSG_CON_DATA_TRANSFER = 3,    // data transfer
    };

    struct MessageFlag
    {
        std::uint8_t raw;

        // FLAG Bits
        // 0: Message Type -- 0: Request, 1: Response
        // 1: Long Message -- 0: Short Message, 1: Long Message
        // 2-3 : Message Content Type, interpreted as an integer
        //  0: Meta Data
        //  1: Drive
        //  2: Switch Data Rate
        //  3: Data Transfer
        // 4-7: Reserved

        MessageFlag() : raw(0) {}

        MessageFlag(MessageType type, MessageContentType content)
        {
            raw = 0;
            raw |= (type << 0);

            switch (content)
            {
            case MSG_CON_META:
                raw |= (0 << 2);
                break;
            case MSG_CON_DRIVE:
                raw |= (1 << 2);
                break;
            case MSG_CON_SWITCH_DATA_RATE:
                raw |= (2 << 2);
                break;
            case MSG_CON_DATA_TRANSFER:
                raw |= (3 << 2);
                break;
            default:
                break;
            }
        }

        MessageType getMessageType() const
        {
            return MessageType((raw >> 0) & 0x1);
        }

        MessageContentType getMessageContentType() const
        {
            return MessageContentType((raw >> 2) & 0x3);
        }

        // equal operator for MessageFlag/uint8_t
        bool operator==(const std::uint8_t &other) const
        {
            return raw == other;
        }

        // equal operator for MessageFlag/MessageFlag
        bool operator==(const MessageFlag &other) const
        {
            return raw == other.raw;
        }

        void markAsLongMessage()
        {
            raw |= BIT_FLAG(1);
        }
    };

    typedef std::vector<std::uint8_t> EncodedMessagePacket;

    class Message
    {
    public:
        static Message createMetaMessageResponse(std::string &schemaName, int major, int minor, int patch)
        {
            std::vector<std::uint8_t> data;
            data.push_back(schemaName.size());
            for (char c : schemaName)
            {
                data.push_back(c);
            }
            data.push_back(major);
            data.push_back(minor);
            data.push_back(patch);
            return Message(MSG_RESPONSE, MSG_CON_META, data);
        }

        static Message createMetaMessageRequest()
        {
            return Message(MSG_REQUEST, MSG_CON_META, std::vector<std::uint8_t>());
        }

        static Message createDriveMessageResponse(const std::string &driveName, const std::string &driveContent)
        {
            std::vector<std::uint8_t> data;
            data.push_back(driveName.size());
            for (char c : driveName)
            {
                data.push_back(c);
            }
            data.push_back(driveContent.size());
            for (char c : driveContent)
            {
                data.push_back(c);
            }

            return Message(MSG_RESPONSE, MSG_CON_DRIVE, data);
        }

        static Message createDriveMessageRequest()
        {
            return Message(MSG_REQUEST, MSG_CON_DRIVE, std::vector<std::uint8_t>());
        }

        static Message createSwitchDataRateMessageRequest(int bandwidth, int frequency)
        {
            std::vector<std::uint8_t> data;
            data.push_back(bandwidth);
            data.push_back(frequency);
            return Message(MSG_REQUEST, MSG_CON_SWITCH_DATA_RATE, data);
        }

        static Message createSwitchDataRateMessageResponse(bool okay)
        {
            std::vector<std::uint8_t> data;
            data.push_back(okay);
            return Message(MSG_RESPONSE, MSG_CON_SWITCH_DATA_RATE, data);
        }

        struct MessageParsingResult
        {
            bool success;
            MessageContentType contentType;
            std::vector<std::uint8_t> data;

            MessageParsingResult(bool success, MessageContentType contentType, std::vector<std::uint8_t> data) : success(success), contentType(contentType), data(data) {}
        };

        static MessageParsingResult decode(const EncodedMessagePacket &packet)
        {
            if (packet.size() < 5)
            {
                std::cout << "Message Parsing Error: Packet size is too small" << std::endl;
                return MessageParsingResult(false, MSG_CON_META, std::vector<std::uint8_t>());
            }

            // check if the packet is a message packet
            for (int i = 0; i < 3; i++)
            {
                if (packet[i] != MSG_IDENTIFIER[i])
                {
                    std::cout << "Message Parsing Error: Invalid message identifier" << std::endl;
                    return MessageParsingResult(false, MSG_CON_META, std::vector<std::uint8_t>());
                }
            }

            MessageFlag flag;
            flag.raw = packet[3];
            std::uint8_t dataSize = packet[4];

            if (dataSize == 0)
            {
                return MessageParsingResult(true, MSG_CON_META, std::vector<std::uint8_t>());
            }

            std::vector<std::uint8_t> payload;
            for (int i = 5; i < packet.size(); i++)
            {
                payload.push_back(packet[i]);
            }

            int headerSize = std::end(MSG_IDENTIFIER) - std::begin(MSG_IDENTIFIER) + 1;

            if (payload.size() != dataSize)
            {
                std::cout << "Message Parsing Error: Data size does not match the packet size" << std::endl;
                std::cout << "Data size: " << payload.size() << " Expected size: " << (unsigned int)dataSize << std::endl;
                return MessageParsingResult(false, MSG_CON_META, std::vector<std::uint8_t>());
            }

            return MessageParsingResult(true, MessageContentType((flag.raw >> 2) & 0x1), payload);
        }

        MessageFlag flag;
        std::vector<std::uint8_t> data;

        std::vector<EncodedMessagePacket> encode() const
        {
            // split the data into packets
            std::vector<EncodedMessagePacket> packets;
            std::vector<std::uint8_t> slice = data;

            while (slice.size() > 0)
            {
                std::cout << "Data size: " << slice.size() << std::endl;
                EncodedMessagePacket packet = this->_buildPacket(slice);
                std::cout << "Packet size: " << packet.size() << std::endl;
                // slice the data
                int offset = (slice.size() > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : slice.size();
                std::cout << "Offset: " << offset << std::endl;
                slice = std::vector<std::uint8_t>(slice.begin() + offset, slice.end());

                packets.push_back(packet);
            }

            return packets;
        }

    private:

        Message(MessageType type, MessageContentType content, const std::vector<std::uint8_t> &data) : flag(MessageFlag(type, content)), data(data) {}

        EncodedMessagePacket _buildPacket(std::vector<std::uint8_t> &data) const
        {
            EncodedMessagePacket packet;
            for (char c : MSG_IDENTIFIER)
            {
                if (c != '\0')
                    packet.push_back(c);
            }

            packet.push_back(flag.raw);

            // make sure the packet size is less than MAX_PACKET_SIZE
            if (data.size() > MAX_PACKET_SIZE)
            {
                std::cout << "Data size is too large for a single packet" << std::endl;
                packet.push_back(0);
                return packet;
            }

            packet.push_back(data.size());
            for (std::uint8_t byte : data)
            {
                packet.push_back(byte);
            }

            return packet;
        }

    };
} // namespace wircom

#endif // __MESSAGE_H__