#if defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41)

#include "com_interface.hpp"
#include <SPI.h>
#include <RH_RF95.h>
#include <unordered_map>

using namespace wircom;

void ComInterface::initialize()
{
    // manually reset the LoRa module
    pinMode(this->_resetPin, OUTPUT);
    digitalWrite(this->_resetPin, HIGH);

    pinMode(this->_resetPin, OUTPUT);
    digitalWrite(this->_resetPin, LOW);
    delay(10);
    digitalWrite(this->_resetPin, HIGH);
    delay(10);

    // initialize the LoRa radio
    if (!this->rf95.init())
    {
        Serial.println("LoRa radio init failed");
        return;
    }

    // set the LoRa radio frequency
    if (!this->rf95.setFrequency(this->_frequency))
    {
        Serial.println("setFrequency failed");
        return;
    }

    // set the transmit power
    this->rf95.setTxPower(this->_power, false);
}

ComInterface ComInterface::addRXCallback(MessageType messageType, MessageContentType contentType, std::function<void(Message)> callback)
{
    std::unordered_map<MessageContentType, std::vector<std::function<void(Message)>>> &callbacks =
        (messageType == MessageType::MSG_REQUEST) ? this->_requestMessageCallbacks : this->_responseMessageCallbacks;

    if (callbacks.find(contentType) == callbacks.end())
    {
        callbacks[contentType] = std::vector<std::function<void(Message)>>();
    }

    callbacks[contentType].push_back(callback);

    return *this;
}

ComInterface ComInterface::addRXCallback(MessageType messageType, std::vector<MessageContentType> contentTypes, std::function<void(Message)> callback)
{
    for (MessageContentType type : contentTypes)
    {
        this->addRXCallback(messageType, type, callback);
    }

    return *this;
}

ComInterface ComInterface::addRXCallbackToAny(MessageType messageType, std::function<void(Message)> callback)
{
    std::vector<MessageContentType> types = {
        MessageContentType::MSG_CON_META,
        MessageContentType::MSG_CON_DRIVE,
        MessageContentType::MSG_CON_SWITCH_DATA_RATE,
        MessageContentType::MSG_CON_DATA_TRANSFER,
    };

    return this->addRXCallback(messageType, types, callback);
}

void ComInterface::switchDataRate(int spreadingFactor, int bandwidth)
{
    this->rf95.setSpreadingFactor(spreadingFactor);
    this->rf95.setSignalBandwidth(bandwidth);
}

void ComInterface::listen(std::uint16_t timeout)
{
    if (this->_radioState != RADIO_STATE_IDLE)
    {
        std::cout << "aborting listen -- the radio is being used" << std::endl;
        return;
    }
    // std::cout << "Listening for messages..." << std::endl;

    unsigned long start = millis();
    this->_radioState = RADIO_STATE_RECEIVING;
    // std::cout << "starting timeout at " << start << std::endl;
    while (millis() - start < timeout)
    {
        // wait for a bit
        // we could be running on a different thread
        // just in case another thread is trying to send a message
        // yield to allow the other thread to run
        // std::cout << "Checking..." << std::endl;
        if (this->_radioState == RADIO_STATE_TRANSMITTING)
        {
            std::cout << "Radio is transmitting, waiting..." << std::endl;
            YIELD;
            continue;
        }

        // check if we have a message
        if (this->rf95.available())
        {
            break;
        }
        YIELD;
    }

    std::cout << "Finished listening" << std::endl;

    this->_radioState = RADIO_STATE_IDLE;

    if (this->rf95.available() == false) return; // nothing 

    std::cout << "Available message..." << std::endl;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (this->rf95.recv(buf, &len))
    {
        std::vector<std::uint8_t> data(buf, buf + len);
        MessageParsingResult res = Message::decode(data);
        if (!res.success)
        {
            std::cout << "Recieved a message, but couldn't parse!" << std::endl;
            return;
        }

        std::cout << "Recieved a message that could be parsed!" << std::endl;
        this->_handleRXMessage(res);
    }
}

void ComInterface::sendMessage(Message msg, bool ackRequired)
{
    RadioState startingState = this->_radioState;
    this->_radioState = RADIO_STATE_TRANSMITTING;

    std::vector<std::vector<std::uint8_t>> packets = msg.encode();
    for (int i = 0; i < packets.size(); i++)
    {
        this->rf95.send(packets[i].data(), packets[i].size());
        this->rf95.waitPacketSent();
    }

    // add the message to the list of messages that require an ack, if the message type requires one
    if (ackRequired && msg.flag.getMessageType() == MessageType::MSG_REQUEST)
    {
        std::cout << "Sending message with ID " << msg.messageID << std::endl;
        std::cout << "Expecting an ack..." << std::endl;
        this->_acksRequired[msg.messageID] = SentMessage{msg, millis(), 0};
    }
    this->_radioState = startingState;
}

void ComInterface::tick()
{
    std::vector<std::uint16_t> toRemove;
    for (auto &sentMessage : this->_acksRequired)
    {
        SentMessage &msg = sentMessage.second;
        std::uint16_t id = sentMessage.first;
        if (millis() - msg.timeSent > SEND_TIMEOUT)
        {
            if (msg.retries < MAX_RETRIES)
            {
                std::cout << "Resending message with ID " << msg.message.messageID
                << " (retry " << (int)msg.retries << ")" << std::endl;
                // resend the message
                this->sendMessage(msg.message, false);
                msg.timeSent = millis();
                msg.retries++;
            }
            else
            {
                // we've reached the max number of retries
                // remove the message from the list
                std::cout << "Message with ID " << msg.message.messageID << " has timed out" << std::endl;
                toRemove.push_back(msg.message.messageID);
            }
        }
    }

    for (std::uint16_t id : toRemove)
    {
        this->_acksRequired.erase(id);
    }

}

void ComInterface::_handleRXMessage(MessageParsingResult res)
{
    std::cout << "res.packetCount " << res.packetCount << std::endl;
    if (res.packetCount == 1)
    {
        std::cout << "Received single packet message of type " << res.contentType << std::endl;
        std::cout << "Message length: " << res.payload.size() << std::endl;
        // this is a normal message, we don't need to collect any more packets
        std::unordered_map<MessageContentType, std::vector<std::function<void(Message)>>> callbacks =
            (res.messageType == MessageType::MSG_REQUEST) ? this->_requestMessageCallbacks : this->_responseMessageCallbacks;

        if (callbacks.find(res.contentType) != callbacks.end())
        {
            Message msg = Message(res.messageID, res.messageType, res.contentType, res.payload);
            for (auto &callback : callbacks[res.contentType])
            {
                callback(msg);
            }
        }

        if (res.messageType == MessageType::MSG_RESPONSE)
        {
            // this is a completed message, mark it as acked
            this->_acksRequired.erase(res.messageID);
        }

        return;
    }

    std::cout
        << "Received packet " << res.packetNumber
        << " of " << res.packetCount
        << " for message type " << res.contentType
        << "for message ID " << res.messageID << std::endl;

    if (this->_messageBuffer.find(res.messageID) == this->_messageBuffer.end())
    {
        this->_messageBuffer[res.messageID] = std::vector<MessageParsingResult>();
    }

    this->_messageBuffer[res.messageID].push_back(res);

    // check if we have all the packets
    if (this->_messageBuffer[res.messageID].size() == this->_messageBuffer[res.messageID][0].packetCount)
    {
        std::vector<MessageParsingResult> packets = this->_messageBuffer[res.messageID];
        std::cout << "Received all packets for message type " << res.contentType << std::endl;
        std::cout << "Collected " << packets.size() << " packets" << std::endl;
        std::vector<std::uint8_t> fullMessage;
        // we need to order the packets by their sequence number
        std::sort(packets.begin(), packets.end(), [](MessageParsingResult a, MessageParsingResult b)
                  { return a.packetNumber < b.packetNumber; });

        for (auto &msg : packets)
        {
            fullMessage.insert(fullMessage.end(), msg.payload.begin(), msg.payload.end());
        }

        std::unordered_map<MessageContentType, std::vector<std::function<void(Message)>>> callbacks =
            (res.messageType == MessageType::MSG_REQUEST) ? this->_requestMessageCallbacks : this->_responseMessageCallbacks;

        if (callbacks.find(res.contentType) != callbacks.end())
        {
            Message msg = Message(res.messageID, res.messageType, res.contentType, fullMessage);
            for (auto &callback : callbacks[res.contentType])
            {
                callback(msg);
            }
        }

        if (res.messageType == MessageType::MSG_RESPONSE)
        {
            // this is a completed message, mark it as acked
            this->_acksRequired.erase(res.messageID);
        }

        this->_messageBuffer.erase(res.messageID);
    }
}

#endif