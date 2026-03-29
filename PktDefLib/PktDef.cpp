#include "../PktDefLib/PktDef.h"
#include <cstring>   // I need this for memcpy

using namespace std;

// Helper function to count the number of 1 bits in a byte
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
    // Header defaults (spec: all header info = 0)
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = HEADERSIZE + 1;   // header + CRC only

    // Body defaults
    packet.bodyType = NONE;
    packet.Data = nullptr;
    packet.dataSize = 0;

    // CRC default (spec: CRC = 0)
    packet.CRC = 0;

    // Raw buffer not allocated yet
    RawBuffer = nullptr;

    // Internal state
    bodySize = 0;
    validPacket = true;
}

PktDef::~PktDef()
{
    // Free serialized buffer if allocated
    if (RawBuffer != nullptr)
    {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }

    // Free message body if allocated
    if (packet.Data != nullptr)
    {
        delete[] packet.Data;
        packet.Data = nullptr;
    }
}

PktDef::PktDef(char* rawData)
{
// This constructor is responsible for parsing the raw data and populating the PktDef object
// This takes in a raw data buffer, parses the data and populates the
// Header, Body, and CRC contents of the PktDef object.
    // Initialize defaults
    RawBuffer = nullptr;
    packet.Data = nullptr;
    packet.dataSize = 0;
    bodySize = 0;
    validPacket = true;

    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = 0;
    packet.bodyType = NONE;
    packet.CRC = 0;

    // Null input ? invalid packet
    if (rawData == nullptr)
    {
        validPacket = false;
        return;
    }

    // ------------------------------
    // 1. Parse header (4 bytes)
    // ------------------------------
    unsigned char low = static_cast<unsigned char>(rawData[0]);
    unsigned char high = static_cast<unsigned char>(rawData[1]);

    packet.header.PktCount = (static_cast<unsigned short>(high) << 8) | static_cast<unsigned short>(low);

    packet.header.Flags = static_cast<unsigned char>(rawData[2]);
    packet.header.Length = static_cast<unsigned char>(rawData[3]);

    // Minimum valid packet = header + CRC
    if (packet.header.Length < HEADERSIZE + 1)
    {
        validPacket = false;
        return;
    }

    // ------------------------------
    // 2. Determine body size
    // ------------------------------
    bodySize = packet.header.Length - HEADERSIZE - 1;

    FlagUnion flags{};
    flags.byte = packet.header.Flags;

    // ------------------------------
    // 3. Determine body type
    // ------------------------------
    if (bodySize == 0)
    {
        packet.bodyType = NONE;
    }
    else if (bodySize == 3 && flags.bits.Drive)
    {
        unsigned char dir = static_cast<unsigned char>(rawData[4]);
        packet.bodyType = (dir == LEFT || dir == RIGHT) ? TURN_BODY : DRIVE_BODY;
    }
    else if (bodySize == 11 && flags.bits.Status)
    {
        packet.bodyType = TELEMETRY_BODY;
    }
    else
    {
        // Could be MESSAGE_BODY, but your current code treats unknown sizes as invalid
        validPacket = false;
        return;
    }

    // ------------------------------
    // 4. Copy body into union
    // ------------------------------
    const unsigned char* d = reinterpret_cast<unsigned char*>(rawData + HEADERSIZE);

    switch (packet.bodyType)
    {
    case DRIVE_BODY:
        packet.body.drive.Direction = d[0];
        packet.body.drive.Duration = d[1];
        packet.body.drive.Power = d[2];
        break;

    case TURN_BODY:
        packet.body.turn.Direction = d[0];
        packet.body.turn.Duration = d[1] | (d[2] << 8);
        break;

    case TELEMETRY_BODY:
        packet.body.telemetry.LastPktCounter = d[0] | (d[1] << 8);
        packet.body.telemetry.CurrentGrade = d[2] | (d[3] << 8);
        packet.body.telemetry.HitCount = d[4] | (d[5] << 8);
        packet.body.telemetry.Heading = d[6] | (d[7] << 8);
        packet.body.telemetry.LastCmd = d[8];
        packet.body.telemetry.LastCmdValue = d[9];
        packet.body.telemetry.LastCmdPower = d[10];
        break;

    case NONE:
    default:
        break;
    }

    // ------------------------------
    // 5. CRC
    // ------------------------------
    packet.CRC = rawData[packet.header.Length - 1];

    if (!CheckCRC(rawData, packet.header.Length))
        validPacket = false;

    // ------------------------------
    // 6. Store raw buffer copy
    // ------------------------------
    RawBuffer = new char[packet.header.Length];
    memcpy(RawBuffer, rawData, packet.header.Length);
}

