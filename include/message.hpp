#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <bitset>
#include <RH_RF95.h> 

#define MSG_IDENTIFIER "NFR"
#define BIT_FLAG(x) (1 << x)
#define MAX_PACKET_SIZE RH_RF95_MAX_MESSAGE_LEN
#define SHORT_MSG_HEADER_SIZE 7
#define LONG_MSG_HEADER_SIZE 9 // for long messages
#define MAX_SHORT_MSG_PAYLOAD_SIZE (MAX_PACKET_SIZE - SHORT_MSG_HEADER_SIZE)
#define MAX_LONG_MSG_PAYLOAD_SIZE (MAX_PACKET_SIZE - LONG_MSG_HEADER_SIZE)

// HEADER STRUCTURE
// 0-2: Identifier
// 3-4: Message ID
// 5: Message Flag
// (if long message)
// 6-7: Packet Number
// 8-9: Packet Count

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

        bool isLongMessage() const
        {
            return (raw & BIT_FLAG(1)) != 0;
        }
    };

    struct MessageParsingResult
    {
        bool success;
        int packetNumber;
        int packetCount;
        std::uint16_t messageID;
        MessageType messageType;
        MessageContentType contentType;
        std::vector<std::uint8_t> payload;

        static MessageParsingResult error()
        {
            return MessageParsingResult(false, 0, MSG_ERROR, MSG_CON_META, std::vector<std::uint8_t>());
        }

        MessageParsingResult(bool success, std::uint16_t id, MessageType messageType, MessageContentType contentType, std::vector<std::uint8_t> data) 
            : success(success), messageID(id), messageType(messageType), contentType(contentType), payload(data), packetNumber(1), packetCount(1) {}
        MessageParsingResult(bool success, std::uint16_t id, int packetNumber, int packetCount, MessageType messageType, MessageContentType contentType, std::vector<std::uint8_t> data) 
            : success(success), messageID(id), packetNumber(packetNumber), packetCount(packetCount), messageType(messageType), contentType(contentType), payload(data) {}
    };

    class Message
    {
    public:
        MessageFlag flag;
        std::vector<std::uint8_t> data;
        std::uint16_t messageID;
        inline static std::uint16_t messageIDCounter;

        Message() : flag(), data(), messageID(0) {}
        Message(const Message &other) : flag(other.flag), data(other.data), messageID(other.messageID) {}
        Message(MessageType type, MessageContentType content, const std::vector<std::uint8_t> &data) : flag(MessageFlag(type, content)), data(data)
        {
            if (data.size() > MAX_SHORT_MSG_PAYLOAD_SIZE)
            {
                this->flag.markAsLongMessage();
            }
            messageID = Message::_getNextMessageID();
        }
        Message(std::uint16_t id, MessageType type, MessageContentType content, const std::vector<std::uint8_t> &data) : flag(MessageFlag(type, content)), data(data), messageID(id) {
            if (data.size() > MAX_SHORT_MSG_PAYLOAD_SIZE)
            {
                this->flag.markAsLongMessage();
            }
        }

        static MessageParsingResult decode(const std::vector<std::uint8_t> &packet);
        static MessageParsingResult decode(const std::vector<std::vector<std::uint8_t>> &packets);
        std::vector<std::vector<std::uint8_t>> encode() const;
        bool operator==(const Message &other) const;

    private:
        std::vector<std::uint8_t> _buildPacket(const std::vector<std::uint8_t> &data, int packetNumber = 0, int packetCount = 0) const;
        static std::uint16_t _getNextMessageID()
        {
            return messageIDCounter++;
        }
    };
} // namespace wircom

#endif // __MESSAGE_H__