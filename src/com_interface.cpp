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

ComInterface ComInterface::addRXCallback(MessageContentType type, std::function<void(std::vector<std::uint8_t>)> callback)
{
    if (this->_rxMessageCallbacks.find(type) == this->_rxMessageCallbacks.end())
    {
        this->_rxMessageCallbacks[type] = std::vector<std::function<void(std::vector<std::uint8_t>)>>();
    }

    this->_rxMessageCallbacks[type].push_back(callback);

    return *this;
}

ComInterface ComInterface::addRXCallback(std::vector<MessageContentType> types, std::function<void(std::vector<std::uint8_t>)> callback)
{
    for (MessageContentType type : types)
    {
        this->addRXCallback(type, callback);
    }

    return *this;
}

ComInterface ComInterface::addRXCallbackToAny(std::function<void(std::vector<std::uint8_t>)> callback)
{
    std::vector<MessageContentType> types = {
        MessageContentType::MSG_CON_META,
        MessageContentType::MSG_CON_DRIVE,
        MessageContentType::MSG_CON_SWITCH_DATA_RATE,
        MessageContentType::MSG_CON_DATA_TRANSFER,
    };

    return this->addRXCallback(types, callback);
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
        return;
    }

    // std::cout << "Listening for messages..." << std::endl;

    if (this->rf95.waitAvailableTimeout(timeout))
    {
        // std::cout << "Available message..." << std::endl;
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);

        if (this->rf95.recv(buf, &len))
        {
            std::vector<std::uint8_t> data(buf, buf + len);
            MessageParsingResult res = Message::decode(data);
            if (!res.success)
            {
                return;
            }

            this->_handleRXMessage(res);
        }
    }
}

void ComInterface::sendMessage(Message msg)
{
    if (this->_radioState != RADIO_STATE_IDLE)
    {
        return;
    }

    this->_radioState = RADIO_STATE_TRANSMITTING;

    std::vector<std::vector<std::uint8_t>> packets = msg.encode();
    for (int i = 0; i < packets.size(); i++)
    {
        this->rf95.send(packets[i].data(), packets[i].size());
        this->rf95.waitPacketSent();
    }

    // add the message to the list of messages that require an ack, if the message type requires one
    this->_acksRequired.push_back(std::make_tuple(msg.flag.getMessageContentType(), msg));

    this->_radioState = RADIO_STATE_IDLE;
}

void ComInterface::_handleRXMessage(MessageParsingResult res)
{
    if (res.packetCount == 1)
    {
        std::cout << "Received single packet message of type " << res.contentType << std::endl;
        std::cout << "Message length: " << res.payload.size() << std::endl;
        // this is a normal message, we don't need to collect any more packets
        if (this->_rxMessageCallbacks.find(res.contentType) != this->_rxMessageCallbacks.end())
        {
            for (auto &callback : this->_rxMessageCallbacks[res.contentType])
            {
                callback(res.payload);
            }
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

        if (this->_rxMessageCallbacks.find(res.contentType) != this->_rxMessageCallbacks.end())
        {
            for (auto &callback : this->_rxMessageCallbacks[res.contentType])
            {
                callback(fullMessage);
            }
        }

        this->_messageBuffer.erase(res.messageID);
    }
}

#endif