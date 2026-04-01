#include "pktdef.h"
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

// ==========Construction and Destruction===========
PktDef::PktDef()
{
    // Header defaults
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = HEADERSIZE + 1;   // header + CRC

    // Body defaults
    packet.bodyType = NONE;
    bodySize = 0;

    // CRC
    packet.CRC = 0;

    // Raw buffer
    RawBuffer = nullptr;

    validPacket = true;
}

PktDef::~PktDef()
{
    if (RawBuffer != nullptr)
    {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }
}

PktDef::PktDef(char* rawData)
{
    RawBuffer = nullptr;
    bodySize = 0;
    validPacket = true;

    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = 0;
    packet.bodyType = NONE;
    packet.CRC = 0;

    if (rawData == nullptr)
    {
        validPacket = false;
        return;
    }

    // 1. Parse header
    unsigned char low = static_cast<unsigned char>(rawData[0]);
    unsigned char high = static_cast<unsigned char>(rawData[1]);

    packet.header.PktCount = (static_cast<unsigned short>(high) << 8) |
        static_cast<unsigned short>(low);
    packet.header.Flags = static_cast<unsigned char>(rawData[2]);
    packet.header.Length = static_cast<unsigned char>(rawData[3]);

    if (packet.header.Length < HEADERSIZE + 1)
    {
        validPacket = false;
        return;
    }

    // 2. Body size
    bodySize = packet.header.Length - HEADERSIZE - 1;

    // Interpret flags via FlagUnion
    FlagUnion IncomingFlag{};
    IncomingFlag.byte = packet.header.Flags;

    // 3. Determine body type
    if (bodySize == 0)
    {
        packet.bodyType = NONE;
    }
    else if (bodySize == 3 && IncomingFlag.bits.Drive)
    {
        unsigned char dir = static_cast<unsigned char>(rawData[4]);

        if (dir == LEFT || dir == RIGHT)
            packet.bodyType = TURN_BODY;
        else
            packet.bodyType = DRIVE_BODY;
    }
    else if (bodySize == 11 && IncomingFlag.bits.Status)
    {
        packet.bodyType = TELEMETRY_BODY;
    }
    else
    {
        validPacket = false;
        return;
    }

    // 4. Copy body into union
    const unsigned char* d = reinterpret_cast<unsigned char*>(rawData + HEADERSIZE);

    switch (packet.bodyType)
    {
    case DRIVE_BODY:
        packet.body.drive.Direction = static_cast<Direction>(d[0]);
        packet.body.drive.Duration = d[1];
        packet.body.drive.Power = d[2];
        break;

    case TURN_BODY:
        packet.body.turn.Direction = static_cast<Direction>(d[0]);
        packet.body.turn.Duration = static_cast<unsigned short>(d[1]) | (static_cast<unsigned short>(d[2]) << 8);
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

    // 5. CRC
    packet.CRC = rawData[packet.header.Length - 1];

    if (!CheckCRC(rawData, packet.header.Length))
        validPacket = false;

    // 6. Store raw buffer copy
    RawBuffer = new char[packet.header.Length];
    std::memcpy(RawBuffer, rawData, packet.header.Length);
}

// ==========Validation Functions===========
bool PktDef::IsValidDirection(Direction dir) const
{
    return (dir == FORWARD || dir == BACKWARD || dir == LEFT || dir == RIGHT);
}

bool PktDef::IsValid() const
{
    return validPacket;
}

bool PktDef::IsDriveCommand() const
{
    FlagUnion Flag{};
    Flag.byte = packet.header.Flags;
    return Flag.bits.Drive;
}

bool PktDef::IsTurnCommand() const
{
    FlagUnion Flag{};
    Flag.byte = packet.header.Flags;

    return Flag.bits.Drive && packet.bodyType == TURN_BODY;
}


bool PktDef::IsTelemetryRequest() const
{
    FlagUnion Flag{};
    Flag.byte = packet.header.Flags;

    return Flag.bits.Status &&
           !Flag.bits.Ack &&
           !Flag.bits.Drive &&
           !Flag.bits.Sleep;
}


bool PktDef::IsAckResponse() const
{
    // Situation #2: ACK Response (Robot → Computer)
    // Valid if:
    //   - Status = 1
    //   - Ack = 1
    bool isStatus = (packet.header.Flags & STATUS_MASK);
    bool hasAck = (packet.header.Flags & ACK_MASK);

    return isStatus && hasAck;
}

bool PktDef::IsTelemetryResponse() const
{
    // Situation #3: Telemetry Response (Robot → Computer)
    // Telemetry must have:
    //   - Status = 1
    //   - Body = 11 bytes (telemetry structure)
    //
    // ACK handling depends on EnforceAckForTelemetry:
    //   - If true:  ACK must be 0
    //   - If false: ACK is ignored (protocol does not specify)
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


// ==========Setters===========
void PktDef::ClearBody()
{
    packet.bodyType = NONE;
    bodySize = 0;

    // Header length = header (4 bytes) + CRC (1 byte)
    packet.header.Length = HEADERSIZE + 1;
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

void PktDef::SetPktCount(int count)
{
    packet.header.PktCount = static_cast<unsigned short>(count);
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


// ==========Getters===========
unsigned char PktDef::GetFlags() const
{
    return packet.header.Flags;
}

int PktDef::GetPktCount() const
{
    return packet.header.PktCount;
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

int PktDef::GetLength() const
{
    return packet.header.Length;
}

bool PktDef::GetAck() const
{
    return (packet.header.Flags & ACK_MASK) != 0;
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