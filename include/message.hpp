#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <cstdint>
#include <string>
#include <vector>

#define MSG_IDENTIFIER "NFR"
#define BIT_FLAG(x) (1 << x)

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

    static MessageFlag create(MessageType type, MessageContentType content)
    {
    }

    void markAsLongMessage()
    {
        flag |= BIT_FLAG(1);
    }
};

typedef EncodedMessagePacket std::vector<std::uint8_t>;

class Message
{
    std::vector<EncodedMessagePacket> encode() const;

private:
    MessageFlag _flag;
    std::vector<std::uint8_t> _data;
};

#endif // __MESSAGE_H__