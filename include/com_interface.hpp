#if defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41)
#ifndef __COM_INTERFACE_H__
#define __COM_INTERFACE_H__

/// com_interface.hpp
/// This file contains the interface for communicating using the LoRa module.
/// It handles basic setup and communication with the LoRa module, and provides
/// callback functions for handling received messages.

#include <SPI.h>
#include <RH_RF95.h>
#include <unordered_map>

#include "message.hpp"

namespace wircom
{
    enum RadioState
    {
        RADIO_STATE_IDLE,
        RADIO_STATE_RECEIVING,
        RADIO_STATE_TRANSMITTING
    };

    /// ComInterface
    /// This class provides an interface for communicating with the LoRa module.
    class ComInterface
    {
    public:
        RH_RF95 rf95;
        bool ready = false;

        ComInterface() : rf95(this->_csPin, this->_interruptPin) {}
        ComInterface(int csPin, int resetPin, int interruptPin, float frequency, int power) : rf95(csPin, interruptPin), _csPin(csPin), _resetPin(resetPin), _interruptPin(interruptPin), _frequency(frequency), _power(power) {}

        void initialize()
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

        /// @brief Adds a callback function for a specific message type.
        /// @param type The message type to add the callback for.
        /// @param callback The callback function to add.
        /// @return this, allowing for chaining of function calls.
        ComInterface addRXCallback(MessageContentType type, std::function<void(std::vector<std::uint8_t>)> callback)
        {
            if (this->_rxMessageCallbacks.find(type) == this->_rxMessageCallbacks.end())
            {
                this->_rxMessageCallbacks[type] = std::vector<std::function<void(std::vector<std::uint8_t>)>>();
            }

            this->_rxMessageCallbacks[type].push_back(callback);

            return *this;
        }

        ComInterface addRXCallback(std::vector<MessageContentType> types, std::function<void(std::vector<std::uint8_t>)> callback)
        {
            for (auto &type : types)
            {
                this->addRXCallback(type, callback);
            }

            return *this;
        }

        ComInterface addRXCallbackToAny(std::function<void(std::vector<std::uint8_t>)> callback)
        {
            std::vector<MessageContentType> types = {
                MessageContentType::MSG_CON_META,
                MessageContentType::MSG_CON_DRIVE,
                MessageContentType::MSG_CON_SWITCH_DATA_RATE,
                MessageContentType::MSG_CON_DATA_TRANSFER,
            };

            return this->addRXCallback(types, callback);
        }

        void switchDataRate(int spreadingFactor, int bandwidth)
        {
            this->rf95.setSpreadingFactor(spreadingFactor);
            this->rf95.setSignalBandwidth(bandwidth);
        }

        void listen()
        {
            if (this->_radioState != RADIO_STATE_IDLE)
            {
                return;
            }

            if (this->rf95.available())
            {
                uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
                uint8_t len = sizeof(buf);

                if (this->rf95.recv(buf, &len))
                {
                    std::vector<std::uint8_t> data(buf, buf + len);
                    Message::MessageParsingResult res = Message::decode(data);
                    if (!res.success)
                    {
                        return;
                    }

                    this->_handleRXMessage(res);
                }
            }
        }

        void sendMessage(Message msg)
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

            this->_radioState = RADIO_STATE_IDLE;
        }

    private:
        std::vector<Message::MessageParsingResult> _messageBuffer;
        std::unordered_map<MessageContentType, std::vector<std::function<void(std::vector<std::uint8_t>)>>> _rxMessageCallbacks;
        RadioState _radioState = RADIO_STATE_IDLE;

        int _expectedPackets = 0;
        int _receivedPackets = 0;

        const int _csPin = 10;
        const int _resetPin = 2;
        const int _interruptPin = 3;
        const float _frequency = 1575.42;
        const int _power = 23;

        void _handleRXMessage(Message::MessageParsingResult res)
        {
            if (res.packetCount == 1)
            {
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

            // we need to collect more packets
            if (this->_messageBuffer.size() == 0)
            {
                this->_expectedPackets = res.packetCount;
            }

            this->_messageBuffer.push_back(res);

            // check if we have all the packets
            if (this->_messageBuffer.size() == this->_expectedPackets)
            {
                std::cout << "Received all packets for message type " << res.contentType << std::endl;
                std::cout << "Collected " << this->_messageBuffer.size() << " packets" << std::endl;
                std::vector<std::uint8_t> fullMessage;
                // we need to order the packets by their sequence number
                std::sort(this->_messageBuffer.begin(), this->_messageBuffer.end(), [](Message::MessageParsingResult a, Message::MessageParsingResult b) {
                    return a.packetNumber < b.packetNumber;
                });

                for (auto &msg : this->_messageBuffer)
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

                this->_messageBuffer.clear();
            }
        }
    };
} // namespace wircom

#endif // __COM_INTERFACE_H__
#endif // defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41)