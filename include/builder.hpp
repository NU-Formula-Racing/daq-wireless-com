#ifndef __BUILDER_H__
#define __BUILDER_H__

#include <string>
#include "message.hpp"
#include "com_interface.hpp"

namespace wircom
{
    class MessageBuilder
    {
    public:
        static Message createMetaMessageResponse(std::uint16_t id, std::string schemaName, int major, int minor, int patch)
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
            return Message(id, MSG_RESPONSE, MSG_CON_META, data);
        }

        static Message createMetaMessageRequest()
        {
            return Message(MSG_REQUEST, MSG_CON_META, std::vector<std::uint8_t>());
        }

        static Message createDriveMessageResponse(std::uint16_t id, const std::string driveContent)
        {
            std::vector<std::uint8_t> data;
            for (char c : driveContent)
            {
                data.push_back(c);
            }
            return Message(id, MSG_RESPONSE, MSG_CON_DRIVE, data);
        }

        static Message createDriveMessageRequest()
        {
            return Message(MSG_REQUEST, MSG_CON_DRIVE, std::vector<std::uint8_t>());
        }

        static Message createSwitchDataRateMessageRequest(int bandwidth, int frequency)
        {
            std::vector<std::uint8_t> payload;
            payload.push_back(bandwidth);
            payload.push_back(frequency);
            return Message(MSG_REQUEST, MSG_CON_SWITCH_DATA_RATE, payload);
        }

        static Message createSwitchDataRateMessageResponse(std::uint16_t id, bool okay)
        {
            std::vector<std::uint8_t> data;
            data.push_back(okay);
            return Message(id, MSG_RESPONSE, MSG_CON_SWITCH_DATA_RATE, data);
        }

        static Message createDataTransferMessage(const std::vector<std::uint8_t> &data)
        {
            return Message(MSG_RESPONSE, MSG_CON_DATA_TRANSFER, data);
        }

        static Message createDataTransferRequest()
        {
            return Message(MSG_REQUEST, MSG_CON_DATA_TRANSFER, std::vector<std::uint8_t>());
        }
    };

// MESSAGE CONTENT STRUCTS
#pragma region MessageContentStructs

struct MetaContent
{
    std::string schemaName;
    int major;
    int minor;
    int patch;
};

struct DriveContent
{
    std::string driveContent;
};

struct SwitchDataRateContent
{
    int bandwidth;
    int frequency;
};

struct DataTransferContent
{
    std::vector<std::uint8_t> data;
};

#pragma endregion

    template <typename T>
    class ContentResult
    {
    public:
        bool success;
        T content;
    };

    class MessageParser
    {
    public:
        static ContentResult<MetaContent> parseMetaContent(const std::vector<std::uint8_t> &data)
        {
            if (data.size() < 4)
            {
                return {false, MetaContent()};
            }

            std::string schemaName;
            int major;
            int minor;
            int patch;

            int schemaNameLength = data[0];
            for (int i = 1; i <= schemaNameLength; i++)
            {
                schemaName.push_back(data[i]);
            }

            major = data[schemaNameLength + 1];
            minor = data[schemaNameLength + 2];
            patch = data[schemaNameLength + 3];

            return {true, MetaContent{schemaName, major, minor, patch}};
        }

        static ContentResult<DriveContent> parseDriveContent(const std::vector<std::uint8_t> &data)
        {
            std::string driveContent;
            for (char c : data)
            {
                driveContent.push_back(c);
            }

            return {true, DriveContent{driveContent}};
        }

        static ContentResult<SwitchDataRateContent> parseSwitchDataRateContent(const std::vector<std::uint8_t> &data)
        {
            if (data.size() < 2)
            {
                return {false, SwitchDataRateContent()};
            }

            int bandwidth = data[0];
            int frequency = data[1];

            return {true, SwitchDataRateContent{bandwidth, frequency}};
        }

        static ContentResult<DataTransferContent> parseDataTransferContent(const std::vector<std::uint8_t> &data)
        {
            return {true, DataTransferContent{data}};
        }
    };
}

#endif // __BUILDER_H__