#define NATIVE

#include <unity.h>
#include <iostream>

#include "message.hpp"

using namespace wircom;

void setUp(void)
{
}

void tearDown(void)
{
}

void test_message_flag()
{
    // test message flag
    MessageFlag flag;
    TEST_ASSERT_EQUAL(0, flag.raw);

    flag = MessageFlag(MessageType::MSG_REQUEST, MessageContentType::MSG_CON_META);
    TEST_ASSERT_EQUAL(MessageType::MSG_REQUEST, flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_META, flag.getMessageContentType());

    flag = MessageFlag(MessageType::MSG_REQUEST, MessageContentType::MSG_CON_DRIVE);
    TEST_ASSERT_EQUAL(MessageType::MSG_REQUEST, flag.getMessageType()); 
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DRIVE, flag.getMessageContentType());

    
    flag = MessageFlag(MessageType::MSG_RESPONSE, MessageContentType::MSG_CON_DATA_TRANSFER);
    TEST_ASSERT_EQUAL(MessageType::MSG_RESPONSE, flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DATA_TRANSFER, flag.getMessageContentType());

    flag = MessageFlag(MessageType::MSG_RESPONSE, MessageContentType::MSG_CON_SWITCH_DATA_RATE);
    TEST_ASSERT_EQUAL(MessageType::MSG_RESPONSE, flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_SWITCH_DATA_RATE, flag.getMessageContentType());
}

void test_meta_message()
{
    std::string schemaName = "Test";
    Message msg = Message::createMetaMessageResponse(schemaName, 1, 0, 1);
    TEST_ASSERT_EQUAL(MessageType::MSG_RESPONSE, msg.flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_META, msg.flag.getMessageContentType());

    // now test that the data is correct
    // should be encoded using run length encoding
    TEST_ASSERT_EQUAL(8, msg.data.size());
    TEST_ASSERT_EQUAL(4, msg.data[0]);
    for (int i = 0; i < schemaName.size(); i++)
    {
        TEST_ASSERT_EQUAL(schemaName[i], msg.data[i + 1]);
    }

    // test the version
    TEST_ASSERT_EQUAL(1, msg.data[5]);
    TEST_ASSERT_EQUAL(0, msg.data[6]);
    TEST_ASSERT_EQUAL(1, msg.data[7]);

    // test encoding
    std::vector<EncodedMessagePacket> packets = msg.encode();
    // test the decoding
    // should be 1 packet, since the data is small
    TEST_ASSERT_EQUAL(1, packets.size());

    std::cout << "Encoded metadata" << std::endl;

    // test the decoding
    Message::MessageParsingResult res = Message::decode(packets[0]);

    std::cout << "Decoded metadata, testing the result" << std::endl;

    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_META, res.contentType);
    TEST_ASSERT_EQUAL(8, res.payload.size());

    std::cout << "Decoded metadata, testing the data" << std::endl;

    // test the data
    TEST_ASSERT_EQUAL(4, res.payload[0]);
    for (int i = 0; i < schemaName.size(); i++)
    {
        TEST_ASSERT_EQUAL(schemaName[i], res.payload[i + 1]);
    }

    // test the version
    TEST_ASSERT_EQUAL(1, res.payload[5]);
    TEST_ASSERT_EQUAL(0, res.payload[6]);
    TEST_ASSERT_EQUAL(1, res.payload[7]);
}

void test_meta_message_request()
{
    Message msg = Message::createMetaMessageRequest();
    TEST_ASSERT_EQUAL(MessageType::MSG_REQUEST, msg.flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_META, msg.flag.getMessageContentType());

    // test the encoding
    std::vector<EncodedMessagePacket> packets = msg.encode();
    // should be 1 packet, since the data is small
    TEST_ASSERT_EQUAL(1, packets.size());

    // test the decoding
    Message::MessageParsingResult res = Message::decode(packets[0]);
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_META, res.contentType);
    TEST_ASSERT_EQUAL(0, res.payload.size());
}

void test_drive_message(void)
{
    std::string content = "meta { .schema : 'test_schema'; .version : 1.0.0; } def TestStruct { float testVal; } def ToSend { TestStruct test; } frame(ToSend)";

    Message msg = Message::createDriveMessageResponse(content);
    TEST_ASSERT_EQUAL(MessageType::MSG_RESPONSE, msg.flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DRIVE, msg.flag.getMessageContentType());

    // test the encoding
    std::vector<EncodedMessagePacket> packets = msg.encode();
    // should be 1 packet, since the data is small
    TEST_ASSERT_EQUAL(1, packets.size());

    // test the decoding
    Message::MessageParsingResult res = Message::decode(packets[0]);
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DRIVE, res.contentType);
    TEST_ASSERT_EQUAL(content.size() + 1, res.payload.size());

    // test the data
    TEST_ASSERT_EQUAL(content.size(), res.payload[0]);
    for (int i = 0; i < content.size(); i++)
    {
        TEST_ASSERT_EQUAL(content[i], res.payload[i + 1]);
    }
}

void test_drive_message_request(void)
{
    Message msg = Message::createDriveMessageRequest();
    TEST_ASSERT_EQUAL(MessageType::MSG_REQUEST, msg.flag.getMessageType());
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DRIVE, msg.flag.getMessageContentType());

    // test the encoding
    std::vector<EncodedMessagePacket> packets = msg.encode();
    // should be 1 packet, since the data is small
    TEST_ASSERT_EQUAL(1, packets.size());

    // test the decoding
    Message::MessageParsingResult res = Message::decode(packets[0]);
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DRIVE, res.contentType);
    TEST_ASSERT_EQUAL(0, res.payload.size());
}

void test_long_message(void)
{
    // create a long message
    std::string content = "";

    for (int i = 0; i < 1000; i++)
    {
        content += "a";
    }

    Message msg = Message::createDriveMessageResponse(content);

    // test the encoding
    std::vector<EncodedMessagePacket> packets = msg.encode();
    // this should not be 1 packet, since the data is large
    TEST_ASSERT_TRUE(packets.size() > 1);

    // test the decoding
    for (int i = 0; i < packets.size(); i++)
    {
        Message::MessageParsingResult res = Message::decode(packets[i]);
        TEST_ASSERT_TRUE(res.success);
        TEST_ASSERT_EQUAL(MessageContentType::MSG_CON_DRIVE, res.contentType);

        if (i == packets.size() - 1)
        {
            TEST_ASSERT_EQUAL(content.size() % MAX_PACKET_SIZE, res.payload.size());
        }
        else
        {
            TEST_ASSERT_EQUAL(MAX_SHORT_MSG_PAYLOAD_SIZE, res.payload.size());
        }

        // test the data
        for (int j = 0; j < res.payload.size(); j++)
        {
            std::cout << "Comparing " << content[i * (MAX_SHORT_MSG_PAYLOAD_SIZE) + j] << " with " << res.payload[j] << std::endl;
            // TEST_ASSERT_EQUAL((std::uint8_t)content[i * (MAX_SHORT_MSG_PAYLOAD_SIZE) + j], res.payload[j]);
        }
    }
}



int main(int argc, char **argv)
{
    UNITY_BEGIN();
    std::cout << "*** RUNNING TESTS ***" << std::endl;
    // run tests
    RUN_TEST(test_message_flag);
    RUN_TEST(test_meta_message);
    RUN_TEST(test_meta_message_request);
    RUN_TEST(test_drive_message);
    RUN_TEST(test_drive_message_request);
    RUN_TEST(test_long_message);

    std::cout << "*** FINISHED RUNNING TESTS ***" << std::endl;
    return UNITY_END();
}