#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <cstdint>
#include <string>
#include <vector>

#define MSG_IDENTIFIER "NFR"
#define BIT_FLAG(x) (1 << x)

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
        std::uint8_t flag;

        // FLAG Bits
        // 0: Message Type -- 0: Request, 1: Response
        // 1: Long Message -- 0: Short Message, 1: Long Message
        // 2: Message Content -- 0: Meta/Drive info, 1: switch data rate, or data transfer
        // 3: Message Content -- if 2 is 0, 0: Meta, 1: Drive, if 2 is 1, 0: Switch Data Rate, 1: Data Transfer
        // 4-7: Reserved

        MessageFlag(MessageType type, MessageContentType content)
        {
            flag = 0;
            flag |= (type << 0);

            switch (content)
            {
            case MSG_CON_META:
                flag |= BIT_FLAG(2);
                break;
            case MSG_CON_DRIVE:
                flag |= BIT_FLAG(2);
                flag |= BIT_FLAG(3);
                break;
            case MSG_CON_SWITCH_DATA_RATE:
                flag |= BIT_FLAG(2);
                break; // flag |= 0
            case MSG_CON_DATA_TRANSFER:
                flag |= BIT_FLAG(2);
                flag |= BIT_FLAG(3);
                break;
            default:
                break;
            }
        }

        void markAsLongMessage()
        {
            flag |= BIT_FLAG(1);
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

        std::vector<EncodedMessagePacket> encode() const;

    private:
        MessageFlag _flag;
        std::vector<std::uint8_t> _data;

        Message(MessageType type, MessageContentType content, const std::vector<std::uint8_t> &data) : _flag(MessageFlag(type, content)), _data(data) {}
    };
} // namespace wircom

#endif // __MESSAGE_H__