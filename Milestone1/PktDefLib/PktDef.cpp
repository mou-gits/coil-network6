#include "pktdef.h"
#include <cstring>   // I need this for memcpy

using namespace std;

static int CountOnBitsInByte(unsigned char value)
{
    int count = 0;

    while (value > 0)
    {
        count += (value & 1);
        value >>= 1;
    }

    return count;
}

PktDef::PktDef()
{
    // Initialize header
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = 0;

    // Initialize body
    packet.Data = nullptr;
    bodySize = 0;

    // Initialize CRC
    packet.CRC = 0;

    // Initialize raw buffer
    RawBuffer = nullptr;

}

PktDef::~PktDef()
{
    // Free dynamically allocated body
    if (packet.Data != nullptr)
    {
        delete[] packet.Data;
        packet.Data = nullptr;
    }

    // Free raw buffer
    if (RawBuffer != nullptr)
    {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }
}

void PktDef::SetPktCount(int count)
{
    packet.header.PktCount = static_cast<unsigned short>(count);
}

// Get packet count
int PktDef::GetPktCount()
{
    return packet.header.PktCount;
}

void PktDef::SetCmd(CmdType cmd)
{
    // Clear only the main command bits: Drive, Status, Sleep
    packet.header.Flags &= ~(DRIVE_MASK | STATUS_MASK | SLEEP_MASK);

    switch (cmd)
    {
    case DRIVE:
        packet.header.Flags = packet.header.Flags | DRIVE_MASK;
        break;

    case SLEEP:
        packet.header.Flags = packet.header.Flags | SLEEP_MASK;
        break;

    case RESPONSE:
        packet.header.Flags = packet.header.Flags | STATUS_MASK;
        break;

    default:
        break;
    }
}


void PktDef::SetBodyData(char* data, int size)
{
    // Delete old body data if it exists
    if (packet.Data != nullptr)
    {
        delete[] packet.Data;
        packet.Data = nullptr;
    }

    // Handle empty or invalid body
    if (data == nullptr || size <= 0)
    {
        bodySize = 0;
        packet.header.Length = HEADERSIZE + 1;   // header + CRC
        return;
    }

    // Allocate new memory
    packet.Data = new char[size];

    // Copy incoming data into packet body
    memcpy(packet.Data, data, size);

    // Store body size
    bodySize = size;

    // Update packet length
    packet.header.Length = HEADERSIZE + bodySize + 1;   
}

CmdType PktDef::GetCmd()
{
    if ((packet.header.Flags & DRIVE_MASK) != 0)
    {
        return DRIVE;
    }
    else if ((packet.header.Flags & SLEEP_MASK) != 0)
    {
        return SLEEP;
    }
    else
    {
        return RESPONSE;
    }
}

bool PktDef::CheckCRC(char* buffer, int size)
{
    if (buffer == nullptr || size <= 1)
    {
        return false;
    }

    int bitCount = 0;

    // Do not include the last byte (used by CRC itself)
    for (int i = 0; i < size - 1; i++)
    {
        bitCount += CountOnBitsInByte((unsigned char)buffer[i]);
    }

    unsigned char calculatedCRC = (unsigned char)(bitCount % 256);
    unsigned char packetCRC = (unsigned char)buffer[size - 1];
    
    return calculatedCRC == packetCRC;
}

void PktDef::CalcCRC()
{
    int bitCount = 0;

    // Count bits in PktCount (2 bytes)
    unsigned short pktCount = packet.header.PktCount;
    unsigned char lowByte = pktCount & 0x00FF;
    unsigned char highByte = (pktCount >> 8) & 0x00FF;

    bitCount += CountOnBitsInByte(lowByte);
    bitCount += CountOnBitsInByte(highByte);

    // Count bits in Flags (1 byte)
    bitCount += CountOnBitsInByte(packet.header.Flags);

    // Count bits in Length (1 byte)
    bitCount += CountOnBitsInByte(packet.header.Length);

    // Count bits in body data (bodySize bytes)
    for (int i = 0; i < bodySize; i++)
    {
        bitCount += CountOnBitsInByte((unsigned char)packet.Data[i]);
    }
    
    // Store result as 1 byte CRC
    packet.CRC = (unsigned char)(bitCount % 256);
}

char* PktDef::GenPacket()
{
    packet.header.Length = HEADERSIZE + bodySize + 1;

    CalcCRC();

    if (RawBuffer != nullptr)
    {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }

    RawBuffer = new char[packet.header.Length];

    // Header 
    RawBuffer[0] = static_cast<char>(packet.header.PktCount & ALL_RIGHT_BITS);
    RawBuffer[1] = static_cast<char>((packet.header.PktCount >> 8) & ALL_RIGHT_BITS);
    RawBuffer[2] = static_cast<char>(packet.header.Flags);
    RawBuffer[3] = static_cast<char>(packet.header.Length);

    // Body 
    if (packet.Data != nullptr && bodySize > 0)
    {
        std::memcpy(RawBuffer + HEADERSIZE, packet.Data, bodySize);
    }

    // CRC
    RawBuffer[packet.header.Length - 1] = packet.CRC;

    return RawBuffer;
}

int PktDef::GetLength()
{
    return packet.header.Length;
}

char* PktDef::GetBodyData()
{
    return packet.Data;
}

bool PktDef::GetAck()
{
    return (packet.header.Flags & ACK_MASK) != 0;
}

PktDef::PktDef(char* rawData) : RawBuffer(nullptr), bodySize(0)
{
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = 0;
    packet.Data = nullptr;
    packet.CRC = 0;

    if (rawData == nullptr)
    {
        return;
    }

    unsigned char lowByte = static_cast<unsigned char>(rawData[0]);
    unsigned char highByte = static_cast<unsigned char>(rawData[1]);

    packet.header.PktCount =
        static_cast<unsigned short>(lowByte) |
        (static_cast<unsigned short>(highByte) << 8);

    packet.header.Flags = static_cast<unsigned char>(rawData[2]);
    packet.header.Length = static_cast<unsigned char>(rawData[3]);

    if (packet.header.Length < HEADERSIZE + 1)
    {
        return;
    }

    bodySize = packet.header.Length - HEADERSIZE - 1;

    if (bodySize > 0)
    {
        packet.Data = new char[bodySize];
        std::memcpy(packet.Data, rawData + HEADERSIZE, bodySize);
    }

    packet.CRC = rawData[packet.header.Length - 1];

    RawBuffer = new char[packet.header.Length];
    std::memcpy(RawBuffer, rawData, packet.header.Length);
}