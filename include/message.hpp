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
        // 2: Message Content -- 0: Meta/Drive info, 1: switch data rate, or data transfer
        // 3: Message Content -- if 2 is 0, 0: Meta, 1: Drive, if 2 is 1, 0: Switch Data Rate, 1: Data Transfer
        // 4-7: Reserved

        MessageFlag() : raw(0) {}

        MessageFlag(MessageType type, MessageContentType content)
        {
            raw = 0;
            raw |= (type << 0);

            switch (content)
            {
            case MSG_CON_META:
                raw |= BIT_FLAG(2);
                break;
            case MSG_CON_DRIVE:
                raw |= BIT_FLAG(2);
                raw |= BIT_FLAG(3);
                break;
            case MSG_CON_SWITCH_DATA_RATE:
                raw |= BIT_FLAG(2);
                break; // flag |= 0
            case MSG_CON_DATA_TRANSFER:
                raw |= BIT_FLAG(2);
                raw |= BIT_FLAG(3);
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
            return MessageContentType((raw >> 2) & 0x1);
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
        static Message createMetaMessage(std::string &schemaName, int major, int minor, int patch)
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

        static Message createDriveMessage(const std::string &driveName, const std::string &driveContent)
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

        static Message createSwitchDataRateMessage(bool okay)
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

        static MessageParsingResult parsePacket(const EncodedMessagePacket &packet)
        {
            if (packet.size() < 5)
            {
                return MessageParsingResult(false, MSG_CON_META, std::vector<std::uint8_t>());
            }

            // check if the packet is a message packet
            for (int i = 0; i < 3; i++)
            {
                if (packet[i] != MSG_IDENTIFIER[i])
                {
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

            std::vector<std::uint8_t> data;
            for (int i = 5; i < packet.size(); i++)
            {
                data.push_back(packet[i]);
            }

            if (data.size() != dataSize)
            {
                std::cout << "Data size does not match the packet size" << std::endl;
                return MessageParsingResult(false, MSG_CON_META, std::vector<std::uint8_t>());
            }

            return MessageParsingResult(true, MessageContentType((flag.raw >> 2) & 0x1), data);
        }

        MessageFlag flag;
        std::vector<std::uint8_t> data;

        std::vector<EncodedMessagePacket> encode() const
        {
            // split the data into packets
            std::vector<EncodedMessagePacket> packets;
            std::vector<std::uint8_t> data = data;

            while (data.size() > 0)
            {
                EncodedMessagePacket packet = this->_buildPacket(data);
                // slice the data
                data = std::vector<std::uint8_t>(data.begin() + MAX_PACKET_SIZE, data.end());
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