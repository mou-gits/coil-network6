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

void PktDef::SetStatus(bool enable)
{
    if (enable)
        packet.header.Flags |= STATUS_MASK;
    else
        packet.header.Flags &= ~STATUS_MASK;
}

void PktDef::SetDriveBody(const DriveBody& body)
{
    // 1. Validate direction
    if (!IsValidDirection(body.Direction))
    {
        ClearBody();
        validPacket = false;
        return;
    }

    // 2. Validate power
    if (body.Power < 80 || body.Power > 100)
    {
        ClearBody();
        validPacket = false;
        return;
    }

    // 3. Normal processing
    char temp[3];
    temp[0] = body.Direction;
    temp[1] = body.Duration;
    temp[2] = body.Power;

    SetBodyData(temp, 3);
}


void PktDef::ClearBody()
{
    if (packet.Data != nullptr)
    {
        delete[] packet.Data;
        packet.Data = nullptr;
    }

    bodySize = 0;
    packet.header.Length = HEADERSIZE + 1;
}

bool PktDef::IsValid() const
{
    return validPacket;
}

bool PktDef::IsValidDirection(unsigned char dir) const
{
    return (dir == FORWARD || dir == BACKWARD ||
        dir == LEFT || dir == RIGHT);
}

void PktDef::SetTurnBody(const TurnBody& body)
{
    // Validate direction: must be LEFT or RIGHT
    if (body.Direction != LEFT && body.Direction != RIGHT)
    {
        ClearBody();
        validPacket = false;
        return;
    }

    // Turn commands always use 100% power 
    // TurnBody is exactly 3 bytes: Direction + Duration(2 bytes)
    char temp[3];
    temp[0] = body.Direction;
    temp[1] = body.Duration & 0xFF;
    temp[2] = (body.Duration >> 8) & 0xFF;

    SetBodyData(temp, 3);
}

unsigned char PktDef::GetFlags() const
{
    return packet.header.Flags;
}

int PktDef::GetBodySize() const
{
    return bodySize;
}


DriveBody PktDef::GetDriveBody() const
{
    DriveBody b{ 0,0,0 };

    if (bodySize == 3)
    {
        b.Direction = packet.Data[0];
        b.Duration = packet.Data[1];
        b.Power = packet.Data[2];
    }

    return b;
}


TurnBody PktDef::GetTurnBody() const
{
    TurnBody b{ 0,0 };

    if (bodySize == 3)
    {
        b.Direction = packet.Data[0];
        b.Duration = (unsigned short)((unsigned char)packet.Data[1] |
            ((unsigned char)packet.Data[2] << 8));
    }

    return b;
}


TelemetryBody PktDef::GetTelemetry() const
{
    TelemetryBody t{ 0 };

    if (bodySize == 11)
    {
        const unsigned char* d = (unsigned char*)packet.Data;

        t.LastPktCounter = d[0] | (d[1] << 8);
        t.CurrentGrade = d[2] | (d[3] << 8);
        t.HitCount = d[4] | (d[5] << 8);
        t.Heading = d[6] | (d[7] << 8);
        t.LastCmd = d[8];
        t.LastCmdValue = d[9];
        t.LastCmdPower = d[10];
    }

    return t;
}


char* PktDef::GetRawBuffer() const
{
    return RawBuffer;
}


bool PktDef::IsDriveCommand() const
{
    return (packet.header.Flags & DRIVE_MASK) != 0;
}

bool PktDef::IsTurnCommand() const
{
    if (!IsDriveCommand()) return false;

    if (bodySize == 3)
    {
        unsigned char dir = packet.Data[0];
        return (dir == LEFT || dir == RIGHT);
    }

    return false;
}

// Situation #1: Telemetry Request (Computer → Robot)
// Valid if:
//   - Status = 1
//   - Ack = 0 (commands must NOT set ACK)
//   - Drive = 0
//   - Sleep = 0
bool PktDef::IsTelemetryRequest() const
{
    bool isStatus = (packet.header.Flags & STATUS_MASK);
    bool hasAck = (packet.header.Flags & ACK_MASK);
    bool isDrive = (packet.header.Flags & DRIVE_MASK);
    bool isSleep = (packet.header.Flags & SLEEP_MASK);

    return isStatus && !hasAck && !isDrive && !isSleep;
}

// Situation #2: ACK Response (Robot → Computer)
// Valid if:
//   - Status = 1
//   - Ack = 1
bool PktDef::IsAckResponse() const
{
    bool isStatus = (packet.header.Flags & STATUS_MASK);
    bool hasAck = (packet.header.Flags & ACK_MASK);

    return isStatus && hasAck;
}


// Situation #3: Telemetry Response (Robot → Computer)
// Telemetry must have:
//   - Status = 1
//   - Body = 11 bytes (telemetry structure)
//
// ACK handling depends on EnforceAckForTelemetry:
//   - If true:  ACK must be 0
//   - If false: ACK is ignored (protocol does not specify)
bool PktDef::IsTelemetryResponse() const
{
    bool isStatus = (packet.header.Flags & STATUS_MASK) != 0;
    bool hasAck = (packet.header.Flags & ACK_MASK) != 0;

    // Telemetry body is always 11 bytes
    bool correctBody = (bodySize == 11);

    if (!isStatus || !correctBody)
        return false;

    // Strict mode: ACK must be 0
    if (ENFORCE_TELEMETRY_ACK_ZERO)
        return !hasAck;

    // Relaxed mode: ACK ignored
    return true;
}


void PktDef::SetPktCount(int count)
{
    packet.header.PktCount = static_cast<unsigned short>(count);
}

// Get packet count
int PktDef::GetPktCount() const
{
    return packet.header.PktCount;
}

void PktDef::SetCmd(CmdType cmd)
{
    packet.header.Flags &= 0xF0; // keep only upper 4 bits

    // Clear only the main command bits: Drive, Status, Sleep
    packet.header.Flags &= ~(DRIVE_MASK | STATUS_MASK | SLEEP_MASK);

    switch (cmd)
    {
    case DRIVE:
        packet.header.Flags = packet.header.Flags | DRIVE_MASK;

        // If body is missing, mark invalid but allow user to set body later
        if (bodySize == 0)
            validPacket = false;

        break;

    case SLEEP:
        packet.header.Flags = packet.header.Flags | SLEEP_MASK;
        ClearBody(); // Sleep must have no body
        break;


    case RESPONSE:
        packet.header.Flags = packet.header.Flags | STATUS_MASK;
        // If not telemetry, clear body
        if (!IsTelemetryResponse())
            ClearBody();
        break;


    default:
        break;
    }
}

void PktDef::SetAck(bool enable)
{
    if (enable)
    {
        // Ack must be paired with a command flag
        if (!(packet.header.Flags & (DRIVE_MASK | SLEEP_MASK | STATUS_MASK)))
        {
            // Default to RESPONSE
            packet.header.Flags |= STATUS_MASK;
        }

        packet.header.Flags |= ACK_MASK;
    }
    else
    {
        packet.header.Flags &= ~ACK_MASK;
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

CmdType PktDef::GetCmd() const
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
    // Enforce: Drive commands must have a body
    if ((packet.header.Flags & DRIVE_MASK) && bodySize == 0)
    {
        validPacket = false;
        // Still generate a minimal packet, but mark invalid
        packet.header.Length = HEADERSIZE + 1;
        CalcCRC();
        return RawBuffer; // or return nullptr if you prefer
    }

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

int PktDef::GetLength() const
{
    return packet.header.Length;
}

char* PktDef::GetBodyData() const
{
    return packet.Data;
}

bool PktDef::GetAck() const
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
    validPacket = true;

    if (rawData == nullptr)
    {
        validPacket = false;
        return;
    }

    // Read header fields (assumes caller provided at least 4 bytes)
    unsigned char lowByte = static_cast<unsigned char>(rawData[0]);
    unsigned char highByte = static_cast<unsigned char>(rawData[1]);

    packet.header.PktCount =
        static_cast<unsigned short>(lowByte) |
        (static_cast<unsigned short>(highByte) << 8);

    packet.header.Flags = static_cast<unsigned char>(rawData[2]);
    packet.header.Length = static_cast<unsigned char>(rawData[3]);

    // Validate minimum legal length
    if (packet.header.Length < HEADERSIZE + 1)
    {
        validPacket = false;
        return;
    }

    bodySize = packet.header.Length - HEADERSIZE - 1;

    if (bodySize > 0)
    {
        packet.Data = new char[bodySize];
        std::memcpy(packet.Data, rawData + HEADERSIZE, bodySize);
    }

    packet.CRC = rawData[packet.header.Length - 1];

    // Validate CRC
    if (!CheckCRC(rawData, packet.header.Length))
    {
        validPacket = false;
    }

    RawBuffer = new char[packet.header.Length];
    std::memcpy(RawBuffer, rawData, packet.header.Length);
}