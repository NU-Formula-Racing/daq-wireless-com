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
// Pin definitions for the LoRa module
#define DEFAULT_RFM95_CS 10 // Chip Select pin
#define DEFAULT_RFM95_RST 2 // Reset pin
#define DEFAULT_RFM95_INT 3 // Interrupt pin
#define SEND_TIMEOUT 1000
#define MAX_RETRIES 3

    enum RadioState
    {
        RADIO_STATE_IDLE,
        RADIO_STATE_RECEIVING,
        RADIO_STATE_TRANSMITTING
    };

    struct SentMessage
    {
        Message message;
        std::uint32_t timeSent;
        std::uint8_t retries;
    };

    /// ComInterface
    /// This class provides an interface for communicating with the LoRa module.
    class ComInterface
    {
    public:
        RH_RF95 rf95;
        bool ready = false;

        ComInterface() : rf95(DEFAULT_RFM95_CS, DEFAULT_RFM95_INT), _csPin(DEFAULT_RFM95_CS), _resetPin(DEFAULT_RFM95_RST), _interruptPin(DEFAULT_RFM95_INT), _frequency(1575.42), _power(23) {}
        ComInterface(int csPin, int resetPin, int interruptPin, float frequency, int power) : rf95(csPin, interruptPin), _csPin(csPin), _resetPin(resetPin), _interruptPin(interruptPin), _frequency(frequency), _power(power) {}

        void initialize();

        /// @brief Adds a callback function for a specific message type.
        /// @param type The message type to add the callback for.
        /// @param callback The callback function to add.
        /// @return this, allowing for chaining of function calls.
        ComInterface addRXCallback(MessageType messageType, MessageContentType contentType, std::function<void(Message)> callback);
        ComInterface addRXCallback(MessageType messageType, std::vector<MessageContentType> contentTypes, std::function<void(Message)> callback);
        ComInterface addRXCallbackToAny(MessageType messageType, std::function<void(Message)> callback);

        void switchDataRate(int spreadingFactor, int bandwidth);

        void listen(std::uint16_t timeout = 1000);
        void sendMessage(Message msg, bool ackRequired = true);
        void tick(); // called in the main loop to handle resending unacked messages

    private:
        std::unordered_map<std::uint16_t, std::vector<MessageParsingResult>> _messageBuffer; // map of message IDs to message packets
        std::unordered_map<MessageContentType, std::vector<std::function<void(Message)>>> _responseMessageCallbacks;
        std::unordered_map<MessageContentType, std::vector<std::function<void(Message)>>> _requestMessageCallbacks;
        volatile RadioState _radioState = RADIO_STATE_IDLE;

        std::unordered_map<std::uint16_t, SentMessage> _acksRequired;

        const int _csPin = 10;
        const int _resetPin = 2;
        const int _interruptPin = 3;
        const float _frequency = 1575.42;
        const int _power = 23;

        void _handleRXMessage(MessageParsingResult res);
        void _markMessageAsAcked(std::uint16_t id);
    };
} // namespace wircom

#endif // __COM_INTERFACE_H__
#endif // defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41)