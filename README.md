<div align="center">
  <img src="docs/img/wircom-logo.png" width=25%>

  <br>

  <a href="https://evanbs.com/portfolio/1/">
	<img src="https://img.shields.io/badge/Blog_Post-blah?style=for-the-badge&logo=checkmarx&logoColor=white&color=243E36" alt="Blog Post"/>
  </a>

</div>

# wircom (daq-wireless-com)

**wircom** (daq-wireless-com) is an application-layer protocol for wireless data acquisition in Northwestern Formula Racing's (NFR) telemetry system. It is designed to be fast, efficient, and reliable, and is intended to run in resource-limited environments, such as microcontrollers. Currently, it is only compatible with the RFM95 LoRa transceiver module, but it can be easily extended to support other modules. Plans are in place to support more modules, and allow for user-defined wireless modules, soon.

wircom was built on top of the [RH_RF95 library](https://www.airspayce.com/mikem/arduino/RadioHead/classRH__RF95.html), which is a driver for the RFM95 LoRa transceiver module. This module powered the wireless communication in [NFR's 2024 telemetry system](https://github.com/NU-Formula-Racing/daq-wireless-24). wircom sits on top of the physical layer provided by the RH_RF95 library and provides a simple, reliable, and efficient way to send and receive data between two devices. It features:

- **Packetization**: wircom breaks data into packets and sends them over the air. This allows for more efficient data transmission and reception. Allows for packets that are larger than the maximum payload size of the RFM95 module, by breaking them into smaller packets.
- **Reliability**: wircom implements a simple, yet effective, acknowledgment system to ensure that data is received correctly. This mechanism allows for the retransmission of lost packets.
- **An easy-to-use API**: wircom provides a simple API for sending and receiving data. Focus on your application, not on the details of wireless communication. Handles the serialization and deserialization of data for you.

This library is heavily integrated with the [daqser](https://github.com/NU-Formula-Racing/daq-serializer-24) library, which is our custom data serialization library. It is used to serialize and deserialize data for transmission over wircom.

## Suggested Network Topology

The wircom library subscribes to a client-server model, where one device acts as the server and the other as the client. The server is responsible for sending data to the client, while the client is responsible for receiving data from the server. This model is suggested for use in NFR's telemetry system, where the server is the car and the client is the pit.

This is showcased in the NFR24 Telemetry System, where the car is the server and the base station is the client. The base station requests data from the car, and the car sends data to the base station.

It is important to note that status as a server or client is not mutually exclusive. Any device can be both a server and a client, depending on the context.

## Installation

This is a PlatformIO project. The easiest way to install is to add this repository as a dependency in your `platformio.ini` file:

```ini
lib_deps = https://github.com/NU-Formula-Racing/daq-wireless-com.git
```

## Usage

### Full Example (Client, Integrated with Daqser)

Here is the source code for the NFR24's remote board, which was located on the car. It acted as the server in the telemetry system.

```cpp
#include <Arduino.h>
#include <virtualTimer.h>
#include <vector>
#include <TeensyThreads.h>

// daqser
#include <daqser.hpp>
#include <daqser_can.hpp>

// wircom
#include <message.hpp>
#include <builder.hpp>
#include <com_interface.hpp>
#include <RH_RF95.h> // dependency of com_interface.hpp, for some reason, it isn't included in the main.cpp file

#include "schema.hpp"

#define VERSION_ARGS(major, minor, patch) major, minor, patch
#define SCHEMA_NAME "daq-schema"
#define SCHEMA_VERSION VERSION_ARGS(1, 0, 0)
#define LISTEN_TIMEOUT 1000

VirtualTimerGroup g_dataTransferTimer;
wircom::ComInterface g_comInterface{};

void listenForMessages()
{
    while (true)
    {
        g_comInterface.listen(LISTEN_TIMEOUT);
        g_comInterface.tick();
    }
}

void onMetaRequest(wircom::Message message)
{
    Serial.println("Meta request received");
    wircom::Message response = wircom::MessageBuilder::createMetaMessageResponse(message.messageID, SCHEMA_NAME, SCHEMA_VERSION);
    g_comInterface.sendMessage(response);
}

void onDriveRequest(wircom::Message message)
{
    // get the contents of the drive file from daqser
    std::string driveFileContents = daqser::getDriveContents();
    // create a message with the drive file contents
    std::cout << "Sending drive file contents..." << std::endl;
    std::cout << driveFileContents << std::endl;
    wircom::Message response = wircom::MessageBuilder::createDriveMessageResponse(message.messageID, driveFileContents);
    // send the message
    g_comInterface.sendMessage(response);
}

void onDataRequest(wircom::Message message)
{
    std::cout << "Sending data..." << std::endl;
    daqser::updateSignals();
    std::cout << "CAN Updated" << std::endl;

    std::vector<std::uint8_t> data = daqser::serializeFrame();
    wircom::Message dmsg = wircom::MessageBuilder::createDataTransferMessage(data);
    g_comInterface.sendMessage(dmsg);
}

void setup()
{
    Serial.begin(9600);

    // Initialize daqser
    std::cout << "Initializing daqser..." << std::endl;
    daqser::initialize();
    daqser::initializeCAN();
    daqser::setSchema(SCHEMA_CONTENTS);

    // Initialize wircom
    std::cout << "Initializing wircom..." << std::endl;
    g_comInterface.initialize();
    threads.addThread(listenForMessages);
    g_comInterface.addRXCallback(
        wircom::MessageType::MSG_REQUEST,
        wircom::MessageContentType::MSG_CON_META,
        onMetaRequest);
    g_comInterface.addRXCallback(
        wircom::MessageType::MSG_REQUEST,
        wircom::MessageContentType::MSG_CON_DRIVE,
        onDriveRequest);
    g_comInterface.addRXCallback(
        wircom::MessageType::MSG_REQUEST,
        wircom::MessageContentType::MSG_CON_DATA_TRANSFER,
        onDataRequest);

    g_dataTransferTimer.AddTimer(10, []()
                                 { daqser::tickCAN(); });
    std::cout << "Finished setup" << std::endl;
}

void loop()
{
    // this is daqser specific
    g_dataTransferTimer.Tick(millis());
}

```

### Breakdown of the Example

#### Initialization

There are a few wircom-specific classes that you need to initialize in your code. The first is the `ComInterface` class. This class is responsible for sending and receiving messages over the air. You can create an instance of this class like so:

```cpp
wircom::ComInterface g_comInterface{};
```

There are also optional parameters that can be passed to the constructor to control the csPin, resetPin, and interruptPin of the RFM95 module. If you do not pass these parameters, the default values will be used. The default values are as follows:

- csPin: 10
- resetPin: 2
- interruptPin: 3

This object will act as the interface between your application and the wircom library. You can use this object to send and receive messages. The best practice is to create a global instance of this object, as it will be used throughout your application. Before you can use the `ComInterface` object, you need to initialize it. You can do this by calling the `initialize` method on the object:

```cpp
g_comInterface.initialize();
```

#### Listening for Messages

After initalizing, you can start listening for messages by calling the `listen` method. This method is blocking. It will wait for a message to be received and then process it. You can specify a timeout value in milliseconds. If no message is received within the timeout period, the method will return. You can call this method in a loop to continuously listen for messages:

```cpp
void listenForMessages()
{
    while (true)
    {
        g_comInterface.listen(LISTEN_TIMEOUT);
        g_comInterface.tick();
    }
}
```

We recommend running this method in a separate thread to avoid blocking the main thread. You can use the TeensyThreads library to create a new thread and run the `listenForMessages` method in that thread:

```cpp
threads.addThread(listenForMessages);
```

#### Receiving Messages

When a message is received, the `listen` method will call the appropriate callback function based on the message type and content type. You can register callback functions for different message types and content types using the `addRXCallback` method. This method takes three arguments: the message type, the content type, and the callback function. The callback function should take a `Message` object as an argument. Here is an example of how to register a callback function for a meta request message:

```cpp
void onMetaRequest(wircom::Message message)
{
    Serial.println("Meta request received");
    wircom::Message response = wircom::MessageBuilder::createMetaMessageResponse(message.messageID, SCHEMA_NAME, SCHEMA_VERSION);
    g_comInterface.sendMessage(response);
}

// somewhere in the setup function
g_comInterface.addRXCallback(
    wircom::MessageType::MSG_REQUEST,
    wircom::MessageContentType::MSG_CON_META,
    onMetaRequest);
```

This code registers the `onMetaRequest` function as the callback function for meta request messages. When a meta request message is received, the `onMetaRequest` function will be called with the message as an argument. In this function, you can process the message and send a response if necessary.

Alternatively, you can use a lambda function as the callback function:

```cpp
g_comInterface.addRXCallback(
    wircom::MessageType::MSG_REQUEST,
    wircom::MessageContentType::MSG_CON_META,
    [](wircom::Message message)
    {
        Serial.println("Meta request received");
        wircom::Message response = wircom::MessageBuilder::createMetaMessageResponse(message.messageID, SCHEMA_NAME, SCHEMA_VERSION);
        g_comInterface.sendMessage(response);
    });
```

There are also a few more helper functions for handling the received messages.

````cpp
// Basic RX Callback
ComInterface addRXCallback(MessageType messageType, MessageContentType contentType, std::function<void(Message)> callback);

// RX Callback for multiple content types
ComInterface addRXCallback(MessageType messageType, std::vector<MessageContentType> contentTypes, std::function<void(Message)> callback);

// RX Callback for any content type
ComInterface addRXCallbackToAny(MessageType messageType, std::function<void(Message)> callback);

````

#### Sending Messages
Sending messages is simple. You can use the `sendMessage` method on the `ComInterface` object to send a message. This method takes a `Message` object as an argument. Here is an example of how to send a meta response message:

```cpp
void onMetaRequest(wircom::Message message)
{
    Serial.println("Meta request received");
    wircom::Message response = wircom::MessageBuilder::createMetaMessageResponse(message.messageID, SCHEMA_NAME, SCHEMA_VERSION);
    g_comInterface.sendMessage(response);
}
```

In our example, this completes the request-response cycle. The client sends a meta request message, and the server responds with a meta response message. The client can then process the response message and display the schema information.

There is also an optional parameter if you care about the reliability of the message, `ackRequired`. If you set this to true, which is the default, the message will be retransmitted until an acknowledgment is received. If you set this to false, the message will be sent once and not retransmitted. This only applies to messages that are sent as a request.

#### Building Message Payloads
If you have noticed, we have been using the `MessageBuilder` class to create message payloads. This class provides a set of static methods to create different types of messages. For example, to create a meta response message, you can use the `createMetaMessageResponse` method:

```cpp
wircom::Message response = wircom::MessageBuilder::createMetaMessageResponse(message.messageID, SCHEMA_NAME, SCHEMA_VERSION);
```

This method takes the message ID, schema name, and schema version as arguments and returns a `Message` object with the appropriate payload. You can then send this message using the `sendMessage` method.

There are a few more helper functions for building messages, in the `MessageBuilder` class.

```cpp
class MessageBuilder
{
public:
    // Builds a meta message response, in response to a meta request. Id should be the same as the request.
    static Message createMetaMessageResponse(std::uint16_t id, std::string schemaName, int major, int minor, int patch);
    // Builds a meta message request
    static Message createMetaMessageRequest();
    // Builds a drive message response, in response to a drive request. Id should be the same as the request.
    static Message createDriveMessageResponse(std::uint16_t id, const std::string driveContent);
    // Builds a drive message request
    static Message createDriveMessageRequest();
    // Builds a switch data rate message request
    static Message createSwitchDataRateMessageRequest(int bandwidth, int frequency);
    // Builds a switch data rate message response, in response to a switch data rate request. Id should be the same as the request.
    static Message createSwitchDataRateMessageResponse(std::uint16_t id, bool okay);
    // Builds a data transfer message, containing the data to be transferred
    static Message createDataTransferMessage(const std::vector<std::uint8_t> &data);
    // Builds a data transfer request message
    static Message createDataTransferRequest();
};
```

#### Parsing Message Payloads
When you receive a message, you can access the payload using the `data` member of the `Message` object. This member is a `std::vector<std::uint8_t>` that contains the raw payload data. You can use this data to extract the information you need. For example, to extract the schema name and version from a meta request message, you can do the following.


```cpp
// callback function for meta response messages
void onRecieveMetaResponse(wircom::Message msg)
{
    std::vector<std::uint8_t> data = msg.data;
    std::cout << "Received message of length: " << data.size() << std::endl;
    // parse the meta content
    wircom::ContentResult<wircom::MetaContent> res = wircom::MessageParser::parseMetaContent(data);
    if (res.success)
    {
        std::cout << "Parsed meta content: " << res.content.schemaName << " " << res.content.major << "." << res.content.minor << "." << res.content.patch << std::endl;
    }
    else
    {
        std::cout << "Failed to parse meta content" << std::endl;
    }
}
```

The data is returned as a generic object `ContentResult`:
```cpp
template <typename T>
class ContentResult
{
public:
    bool success;
    T content;
};
```

This object contains a boolean `success` field, which indicates whether the parsing was successful, and a `content` field, which contains the parsed content. The `content` field is of type `T`, which is a template parameter. In the example above, `T` is `MetaContent`, which is a struct that contains the schema name and version.

The `MessageParser` class provides a set of static methods to parse different types of message payloads. For example, to parse the meta content of a message, you can use the `parseMetaContent` method:

```cpp
wircom::ContentResult<wircom::MetaContent> res = wircom::MessageParser::parseMetaContent(data);
```

This method takes the raw payload data as an argument and returns a `ContentResult` object containing the parsed content. You can then access the content using the `content` field of the `ContentResult` object.

There are a few more helper functions for parsing messages, in the `MessageParser` class.

```cpp
class MessageParser
{
public:
    // Parses the meta content of a message
    static ContentResult<MetaContent> parseMetaContent(const std::vector<std::uint8_t> &data);
    // Parses the drive content of a message
    static ContentResult<DriveContent> parseDriveContent(const std::vector<std::uint8_t> &data);
    // Parses the switch data rate content of a message
    static ContentResult<SwitchDataRateContent> parseSwitchDataRateContent(const std::vector<std::uint8_t> &data);
    // Parses the data transfer content of a message
    static ContentResult<DataTransferContent> parseDataTransferContent(const std::vector<std::uint8_t> &data);
};
```

## License & Acknowledgements

This project is licensed under the MIT License - see the [LICENSE](LICENSE.txt) file for details.

This library was developed solely by [Evan Bertis-Sample](https://evanbs.com) for Northwestern Formula Racing. With dependencies on the [RH_RF95 library](https://www.airspayce.com/mikem/arduino/RadioHead/classRH__RF95.html).

And of course, none of this would be possible (or at least, useful) without the tireless work of my teammates, and friends, at Northwestern Formula Racing. Thank you for all of your support and guidance.

![Team Photo](docs/img/team-photo.jpg)