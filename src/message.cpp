// message.hpp

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <bitset>

#include "message.hpp"

using namespace wircom;

MessageParsingResult Message::decode(const std::vector<std::uint8_t> &packet)
{
    if (packet.size() < SHORT_MSG_HEADER_SIZE)
    {
        std::cout << "Message Parsing Error: Packet size is too small" << std::endl;
        return MessageParsingResult::error();
    }

    // check if the packet is a message packet
    for (int i = 0; i < 3; i++)
    {
        if (packet[i] != MSG_IDENTIFIER[i])
        {
            std::cout << "Message Parsing Error: Invalid message identifier" << std::endl;
            return MessageParsingResult::error();
        }
    }

    MessageFlag flag;
    std::uint16_t messageID = (packet[3] << 8) | packet[4];
    flag.raw = packet[5];

    // print the flag bits
    std::cout << "Flag bits: " << std::bitset<8>(flag.raw) << std::endl;
    std::cout << "Message Type: " << flag.getMessageType() << std::endl;
    std::cout << "Content Type: " << flag.getMessageContentType() << std::endl;
    int payloadStart = SHORT_MSG_HEADER_SIZE - 1; // assume short message

    if (flag.isLongMessage())
    {
        std::cout << "Long message detected" << std::endl;
        payloadStart = LONG_MSG_HEADER_SIZE - 1;
    }

    if (packet.size() <= payloadStart)
    {
        std::cout << "Message Parsing Error: Packet size is too small" << std::endl;
        return MessageParsingResult::error();
    }

    std::uint8_t dataSize = packet[payloadStart];

    if (dataSize == 0)
    {
        // this has no payload
        std::cout << "Message Parsing: No payload" << std::endl;
        return MessageParsingResult(true, messageID, flag.getMessageType(), flag.getMessageContentType(), std::vector<std::uint8_t>());
    }

    std::vector<std::uint8_t> payload;
    for (int i = payloadStart + 1; i < packet.size(); i++)
    {
        payload.push_back(packet[i]);
    }

    if (payload.size() != dataSize)
    {
        std::cout << "Message Parsing Error: Data size does not match the packet size" << std::endl;
        std::cout << "Data size: " << payload.size() << " Expected size: " << (unsigned int)dataSize << std::endl;
        return MessageParsingResult::error();
    }

    if (flag.isLongMessage())
    {
        return MessageParsingResult(true, messageID, packet[payloadStart - 2], packet[payloadStart - 1], flag.getMessageType(), flag.getMessageContentType(), payload);
    }

    return MessageParsingResult(true, messageID, flag.getMessageType(), flag.getMessageContentType(), payload);
}

MessageParsingResult Message::decode(const std::vector<std::vector<std::uint8_t>> &packets)
{
    std::vector<std::uint8_t> payload;
    for (const std::vector<std::uint8_t> &packet : packets)
    {
        for (std::uint8_t byte : packet)
        {
            payload.push_back(byte);
        }
    }

    return decode(payload);
}

bool Message::operator==(const Message &other) const
{
    if (flag == other.flag && data.size() == other.data.size() && messageID == other.messageID)
    {
        for (int i = 0; i < data.size(); i++)
        {
            if (data[i] != other.data[i])
            {
                return false;
            }
        }
        return true;
    }

    return false;
}

std::vector<std::vector<std::uint8_t>> Message::encode() const
{
    // split the data into packets
    std::vector<std::vector<std::uint8_t>> packets;
    std::vector<std::uint8_t> slice = data;

    if (slice.size() == 0)
    {
        // no data to send
        std::vector<std::uint8_t> packet = this->_buildPacket(slice);
        packets.push_back(packet);
        return packets;
    }

    std::uint8_t numPackets = 1;
    if (flag.isLongMessage())
    {
        std::cout << "Encoding::Long message detected" << std::endl;
        // this a long message, it needs to be split into multiple packets
        float fNumPackets = (float)slice.size() / MAX_LONG_MSG_PAYLOAD_SIZE;
        if (fNumPackets > (std::uint8_t)fNumPackets)
        {
            numPackets = (std::uint8_t)fNumPackets + 1;
        }
        else
        {
            numPackets = (std::uint8_t)fNumPackets;
        }
    }
    std::uint8_t packetIndex = 0;
    int maxPayloadSize = (flag.isLongMessage()) ? MAX_LONG_MSG_PAYLOAD_SIZE : MAX_SHORT_MSG_PAYLOAD_SIZE;

    // std::cout << "Number of packets: " << numPackets << std::endl;

    while (slice.size() > 0)
    {
        // std::cout << "Data size: " << slice.size() << std::endl;
        // slice the data
        int offset = (slice.size() > maxPayloadSize) ? maxPayloadSize : slice.size();
        // std::cout << "Offset: " << offset << std::endl;
        std::vector<std::uint8_t> packetData = std::vector<std::uint8_t>(slice.begin(), slice.begin() + offset);
        std::vector<std::uint8_t> packet = this->_buildPacket(packetData, packetIndex, numPackets);

        slice = std::vector<std::uint8_t>(slice.begin() + offset, slice.end());

        packets.push_back(packet);
        packetIndex++;
    }

    return packets;
}

std::vector<std::uint8_t> Message::_buildPacket(const std::vector<std::uint8_t> &data, std::uint8_t packetNumber, std::uint8_t packetCount) const
{
    std::vector<std::uint8_t> packet;
    for (char c : MSG_IDENTIFIER)
    {
        if (c != '\0')
            packet.push_back(c);
    }

    // add the message ID
    packet.push_back((messageID >> 8) & 0xFF);
    packet.push_back(messageID & 0xFF);
    packet.push_back(flag.raw);

    // print the flag bits
    // std::cout << "Building packet: " << std::endl;
    // std::cout << "  Flag bits: " << std::bitset<8>(flag.raw) << std::endl;

    // make sure the packet size is less than MAX_PACKET_SIZE
    if (data.size() > MAX_SHORT_MSG_PAYLOAD_SIZE && !flag.isLongMessage())
    {
        // std::cout << "Data size is too large for a single packet" << std::endl;
        packet.push_back(0);
        return packet;
    }
    else if (data.size() > MAX_LONG_MSG_PAYLOAD_SIZE && flag.isLongMessage())
    {
        // std::cout << "Data size is too large for a single packet" << std::endl;
        packet.push_back(0);
        return packet;
    }

    if (packetCount > 1)
    {
        std::cout << "  Packet number: " << packetNumber << " Packet count: " << packetCount << std::endl;
        packet.push_back(packetNumber);
        packet.push_back(packetCount);
    }

    packet.push_back(data.size());

    for (std::uint8_t byte : data)
    {
        packet.push_back(byte);
    }

    return packet;
}
