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
    Message msg = Message::createMetaMessage(schemaName, 1, 0, 1);
    TEST_ASSERT_EQUAL(0, msg.flag.raw);


}



int main(int argc, char **argv)
{
    UNITY_BEGIN();
    std::cout << "*** RUNNING TESTS ***" << std::endl;
    // run tests
    RUN_TEST(test_message_flag);
    RUN_TEST(test_meta_message);

    std::cout << "*** FINISHED RUNNING TESTS ***" << std::endl;
    return UNITY_END();
}