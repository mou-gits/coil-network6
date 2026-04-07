#include "../PktDefLib/PktDef.h"
#include <cstring>
#include <cstdint>

using namespace std;

// Helper function to count the number of 1 bits in a byte
static int CountBits(unsigned char value)
{
    int count = 0;
    while (value) {
        count += (value & 1);
        value >>= 1;
    }
    return count;
}

inline unsigned char BYTE(uint16_t x, int n)
{
    return static_cast<unsigned char>((x >> (8 * n)) & 0xFF);
}

// Special functions
bool PktDef::LooksLikeDriveBody() const
{
    if (bodySize != sizeof(DriveBody))
        return false;

    const DriveBody* d = nullptr;

    if (packet.Data != nullptr)
        d = reinterpret_cast<const DriveBody*>(packet.Data);   // incoming
    else
        d = &packet.body.drive;                                // outgoing

    bool validDirection = (d->Direction >= 1 && d->Direction <= 4);
    bool validPower = (d->Power >= 80 && d->Power <= 100);

    return validDirection && validPower;
}




bool PktDef::LooksLikeTurnBody() const
{
    if (bodySize != sizeof(TurnBody))
        return false;

    const TurnBody* t = nullptr;

    if (packet.Data != nullptr)
        t = reinterpret_cast<const TurnBody*>(packet.Data);   // incoming
    else
        t = &packet.body.turn;                                // outgoing

    bool validTurnDir = (t->Direction == LEFT || t->Direction == RIGHT);

    return validTurnDir;
}


bool PktDef::LooksLikeTelemetryBody() const
{
    if (bodySize != sizeof(TelemetryBody))
        return false;

    const TelemetryBody* t = nullptr;

    if (packet.Data != nullptr)
        t = reinterpret_cast<const TelemetryBody*>(packet.Data);   // incoming
    else
        t = &packet.body.telemetry;                                // outgoing

    if (t->Heading > 359) return false;
    if (t->LastCmdPower > 100) return false;

    return true;
}



// Constructors and Destructors
PktDef::PktDef()
{
    // Header defaults (spec: all header info = 0)
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = HEADERSIZE + 1;   // header + CRC only

    // Body defaults
    packet.bodyType = NONE;
    packet.Data = nullptr;
    packet.message = nullptr;
    packet.dataSize = 0;

    // CRC default (spec: CRC = 0)
    packet.CRC = 0;

    // Raw buffer not allocated yet
    RawBuffer = nullptr;

    // Internal state
    bodySize = 0;
    validPacket = false;
}

PktDef::PktDef(char* raw)
{
    // ---------------------------------------------------------
    // 0. Initialize safe state
    // ---------------------------------------------------------
    bodySize = 0;
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = 0;

    packet.bodyType = BodyType::NONE;
    packet.Data = nullptr;
    packet.message = nullptr;
    packet.dataSize = 0;
    packet.CRC = 0;
    RawBuffer = nullptr;
    validPacket = false;

    if (raw == nullptr)
        return;

    // -----------------------------------------------------------------
    // 1. Parse header (first 4 bytes)
    // -----------------------------------------------------------------
    packet.header.PktCount = (static_cast<unsigned char>(raw[0]) << 8) | (static_cast<unsigned char>(raw[1]));
    packet.header.Flags = static_cast<unsigned char>(raw[2]);
    packet.header.Length = static_cast<unsigned char>(raw[3]);

    // -----------------------------------------------------------------
    // 2. Compute body size
    // -----------------------------------------------------------------
    int totalLength = packet.header.Length;
    bodySize = totalLength - HEADERSIZE - 1;   // minus CRC

    if (bodySize < 0)
        return; // invalid packet

    packet.dataSize = bodySize;

    // -----------------------------------------------------------------
    // 3. Copy raw body bytes
    // -----------------------------------------------------------------
    if (bodySize > 0)
    {
        packet.Data = new char[bodySize];
        memcpy(packet.Data, raw + HEADERSIZE, bodySize);
    }

    // -----------------------------------------------------------------
    // 4. Read CRC (last byte)
    // -----------------------------------------------------------------
    packet.CRC = static_cast<unsigned char>(raw[HEADERSIZE + bodySize]);

    // -----------------------------------------------------------------
    // 5. Copy full raw buffer for CRC checking
    // -----------------------------------------------------------------
    RawBuffer = new char[packet.header.Length];
    memcpy(RawBuffer, raw, packet.header.Length);

    // -----------------------------------------------------------------
    // 6. Classify packet
    // -----------------------------------------------------------------
    PacketClass cls = ClassifyPacket();

    // -----------------------------------------------------------------
    // 7. Populate bodyType, body, and message based on classification
    // -----------------------------------------------------------------
    switch (cls)
    {
    case PacketClass::DriveCommand:
        packet.bodyType = DRIVE_BODY;
        if (packet.Data && packet.dataSize >= static_cast<int>(sizeof(DriveBody)))
            memcpy(&packet.body.drive, packet.Data, sizeof(DriveBody));
        break;

    case PacketClass::TurnCommand:
        packet.bodyType = TURN_BODY;
        if (packet.Data && packet.dataSize >= static_cast<int>(sizeof(TurnBody)))
            memcpy(&packet.body.turn, packet.Data, sizeof(TurnBody));
        break;

    case PacketClass::TelemetryResponse:
        packet.bodyType = TELEMETRY_BODY;
        if (packet.Data && packet.dataSize >= static_cast<int>(sizeof(TelemetryBody)))
            memcpy(&packet.body.telemetry, packet.Data, sizeof(TelemetryBody));
        break;

    case PacketClass::SleepCommand:
    case PacketClass::TelemetryCommand:
        packet.bodyType = NONE;
        break;

    case PacketClass::AckResponse:
    case PacketClass::NAckResponse:
        packet.bodyType = MESSAGE_BODY;

        if (packet.dataSize > 0 && packet.Data != nullptr)
        {
            packet.message = new char[packet.dataSize + 1];
            memcpy(packet.message, packet.Data, packet.dataSize);
            packet.message[packet.dataSize] = '\0';
        }
        break;

    default:
        packet.bodyType = NONE;
        break;
    }

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

    // Free raw data buffer if allocated
    if (packet.Data != nullptr)
    {
        delete[] packet.Data;
        packet.Data = nullptr;
    }

    // Free message body if allocated
    if (packet.message != nullptr)
    {
        delete[] packet.message;
        packet.message = nullptr;
    }
}

// Packet Generation
char* PktDef::GenPacket()
{
    // ---------------------------------------------------------
    // 0. Free old buffer
    // ---------------------------------------------------------
    if (RawBuffer != nullptr)
    {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }

    // ---------------------------------------------------------
    // 1. Compute body size based on bodyType
    // ---------------------------------------------------------
    switch (packet.bodyType)
    {
    case DRIVE_BODY:
        bodySize = sizeof(DriveBody);
        break;

    case TURN_BODY:
        bodySize = sizeof(TurnBody);
        break;

    case TELEMETRY_BODY:
        bodySize = sizeof(TelemetryBody);
        break;

    case MESSAGE_BODY:
        bodySize = packet.dataSize;   // message length
        break;

    case NONE:
    default:
        bodySize = 0;
        break;
    }

    packet.dataSize = bodySize;

    // ---------------------------------------------------------
    // 2. Compute total packet length
    // ---------------------------------------------------------
    packet.header.Length = HEADERSIZE + bodySize + 1; // + CRC

    // ---------------------------------------------------------
    // 3. Allocate RawBuffer
    // ---------------------------------------------------------
    RawBuffer = new char[packet.header.Length];
    int index = 0;

    // ---------------------------------------------------------
    // 4. Write Header
    // ---------------------------------------------------------
    RawBuffer[index++] = static_cast<char>(BYTE(packet.header.PktCount, 1)); // high byte
    RawBuffer[index++] = static_cast<char>(BYTE(packet.header.PktCount, 0)); // low byte
    RawBuffer[index++] = static_cast<char>(packet.header.Flags);
    RawBuffer[index++] = static_cast<char>(packet.header.Length);

    // ---------------------------------------------------------
    // 5. Write Body
    // ---------------------------------------------------------
    if (packet.bodyType == DRIVE_BODY)
    {
        RawBuffer[index++] = packet.body.drive.Direction;
        RawBuffer[index++] = packet.body.drive.Duration;
        RawBuffer[index++] = packet.body.drive.Power;
    }
    else if (packet.bodyType == TURN_BODY)
    {
        RawBuffer[index++] = packet.body.turn.Direction;
        RawBuffer[index++] = static_cast<char>(packet.body.turn.Duration & 0xFF);
        RawBuffer[index++] = static_cast<char>((packet.body.turn.Duration >> 8) & 0xFF);
    }
    else if (packet.bodyType == TELEMETRY_BODY)
    {
        memcpy(RawBuffer + index, &packet.body.telemetry, sizeof(TelemetryBody));
        index += sizeof(TelemetryBody);
    }
    else if (packet.bodyType == MESSAGE_BODY)
    {
        if (packet.message != nullptr && packet.dataSize > 0)
        {
            memcpy(RawBuffer + index, packet.message, packet.dataSize);
            index += packet.dataSize;
        }
    }
    // NONE ? write nothing

    // ---------------------------------------------------------
    // 6. Compute CRC
    // ---------------------------------------------------------
    CalcCRC();

    // ---------------------------------------------------------
    // 7. Write CRC
    // ---------------------------------------------------------
    RawBuffer[index] = static_cast<char>(packet.CRC);

    validPacket = true;
    return RawBuffer;
}

// Classifiers and Validators
bool PktDef::IsValidDirection(Direction dir) const
{
    return (dir == FORWARD ||
        dir == BACKWARD ||
        dir == LEFT ||
        dir == RIGHT);
}

PacketClass PktDef::ClassifyPacket() const
{
    FlagUnion fu;
    fu.byte = packet.header.Flags;

    int body_size = packet.dataSize;

    // ---------------------------------------------------------
    // OUTGOING CLASSIFICATION (packet.Data == nullptr)
    // ---------------------------------------------------------
    if (packet.Data == nullptr)
    {
        // Basic sanity
        if (packet.header.Length < HEADERSIZE + 1)
            return PacketClass::Invalid;

        // Commands with no body
        if (packet.bodyType == NONE)
        {
            if (fu.bits.Ack == 1)
                return PacketClass::AckResponse;

            if (fu.bits.Sleep == 1 && fu.bits.Ack == 0)
                return PacketClass::SleepCommand;

            if (fu.bits.Status == 1 && fu.bits.Ack == 0)
                return PacketClass::TelemetryCommand;

            if (fu.bits.Ack == 0 && (fu.bits.Drive || fu.bits.Status || fu.bits.Sleep))
                return PacketClass::NAckResponse;

            return PacketClass::Invalid;
        }

        // Telemetry response
        // Some robot implementations set Ack=1 for telemetry replies.
        // Accept telemetry-shaped status packets regardless of Ack bit.
        if (fu.bits.Status == 1 && LooksLikeTelemetryBody())
            return PacketClass::TelemetryResponse;

        // Drive / Turn
        if (fu.bits.Drive == 1 && fu.bits.Ack == 0)
        {
            if (packet.bodyType == DRIVE_BODY)
                return PacketClass::DriveCommand;

            if (packet.bodyType == TURN_BODY)
                return PacketClass::TurnCommand;

            return PacketClass::NAckResponse;
        }

        // ACK/NACK with message
        if (packet.bodyType == MESSAGE_BODY)
        {
            return (fu.bits.Ack == 1)
                ? PacketClass::AckResponse
                : PacketClass::NAckResponse;
        }

        return PacketClass::Invalid;
    }

    // ---------------------------------------------------------
    // INCOMING CLASSIFICATION (raw Data available)
    // ---------------------------------------------------------

    // 1. Basic sanity
    if (packet.header.Length < HEADERSIZE + 1)
        return PacketClass::Invalid;

    if (body_size < 0)
        return PacketClass::Invalid;

    // 2. Commands with NO BODY
    if (body_size == 0)
    {
        if (fu.bits.Ack == 1)
            return PacketClass::AckResponse;

        if (fu.bits.Sleep == 1 && fu.bits.Ack == 0)
            return PacketClass::SleepCommand;

        if (fu.bits.Status == 1 && fu.bits.Ack == 0)
            return PacketClass::TelemetryCommand;

        if (fu.bits.Ack == 0 && (fu.bits.Drive || fu.bits.Status || fu.bits.Sleep))
            return PacketClass::NAckResponse;

        return PacketClass::Invalid;
    }

    // 3. Telemetry Response
    // Some robot implementations set Ack=1 for telemetry replies.
    // Accept telemetry-shaped status packets regardless of Ack bit.
    if (fu.bits.Status == 1)
    {
        if (LooksLikeTelemetryBody())
            return PacketClass::TelemetryResponse;

        if (fu.bits.Ack == 0)
            return PacketClass::NAckResponse;
    }

    // 4. Drive / Turn Commands
    if (fu.bits.Drive == 1 && fu.bits.Ack == 0)
    {
        if (LooksLikeDriveBody())
            return PacketClass::DriveCommand;

        if (LooksLikeTurnBody())
            return PacketClass::TurnCommand;

        return PacketClass::NAckResponse;
    }

    // 5. ACK
    if (fu.bits.Ack == 1)
        return PacketClass::AckResponse;

    // 6. Fallback: NACK with message
    return PacketClass::NAckResponse;
}


bool PktDef::IsDriveCommand() const {
    return ClassifyPacket() == PacketClass::DriveCommand;
}

bool PktDef::IsTurnCommand() const {
    return ClassifyPacket() == PacketClass::TurnCommand;
}

bool PktDef::IsSleepCommand() const {
    return ClassifyPacket() == PacketClass::SleepCommand;
}

bool PktDef::IsTelemetryCommand() const {
    return ClassifyPacket() == PacketClass::TelemetryCommand;
}

bool PktDef::IsAckResponse() const {
    return ClassifyPacket() == PacketClass::AckResponse;
}

bool PktDef::IsNAckResponse() const {
    return ClassifyPacket() == PacketClass::NAckResponse;
}

bool PktDef::IsTelemetryResponse() const {
    return ClassifyPacket() == PacketClass::TelemetryResponse;
}

bool PktDef::IsValid() const
{
    if (!validPacket)
        return false;

    if (RawBuffer == nullptr)
        return false;

    if (!CheckCRC(RawBuffer, packet.header.Length))
        return false;

    if (packet.header.Length != HEADERSIZE + bodySize + 1)
        return false;

    FlagUnion flags{};
    flags.byte = packet.header.Flags;

    int cmdCount = flags.bits.Drive + flags.bits.Status + flags.bits.Sleep;
    if (cmdCount > 1)
        return false;

    PacketClass cls = ClassifyPacket();

    // Commands cannot have Ack=1
    if ((cls == PacketClass::DriveCommand ||
        cls == PacketClass::TurnCommand ||
        cls == PacketClass::SleepCommand ||
        cls == PacketClass::TelemetryCommand) &&
        flags.bits.Ack == 1)
    {
        return false;
    }

    switch (cls)
    {
    case PacketClass::DriveCommand:
        if (!LooksLikeDriveBody())
            return false;
        return true;

    case PacketClass::TurnCommand:
        if (!LooksLikeTurnBody())
            return false;
        return true;

    case PacketClass::SleepCommand:
    case PacketClass::TelemetryCommand:
        return (bodySize == 0);

    case PacketClass::TelemetryResponse:
        if (!LooksLikeTelemetryBody())
            return false;
        return true;

    case PacketClass::AckResponse:
    case PacketClass::NAckResponse:
        // ACK/NACK must NOT contain structured bodies
        if (LooksLikeDriveBody() ||
            LooksLikeTurnBody() ||
            LooksLikeTelemetryBody())
        {
            return false;
        }
        return true;

    default:
        return false;
    }
}


void PktDef::CalcCRC()
{
    if (RawBuffer == nullptr)
        return;

    int crc = 0;

    // Count bits in all bytes except the CRC byte
    for (int i = 0; i < packet.header.Length - 1; i++)
    {
        crc += CountBits(static_cast<unsigned char>(RawBuffer[i]));
    }

    packet.CRC = static_cast<unsigned char>(crc);
}

bool PktDef::CheckCRC(char* buffer, int size) const
{
    if (buffer == nullptr || size <= 1)
        return false;

    int crc = 0;

    // Count bits in all bytes except the last (CRC)
    for (int i = 0; i < size - 1; i++)
    {
        crc += CountBits(static_cast<unsigned char>(buffer[i]));
    }

    unsigned char expected = static_cast<unsigned char>(crc);
    unsigned char actual = static_cast<unsigned char>(buffer[size - 1]);

    return expected == actual;
}

// Internal Helper
void PktDef::ClearBody()
{
    // Clear fixed body
    memset(&packet.body, 0, sizeof(packet.body));

    // Clear variable body (raw data)
    if (packet.Data != nullptr)
    {
        delete[] packet.Data;
        packet.Data = nullptr;
    }

    // Clear message body
    if (packet.message != nullptr)
    {
        delete[] packet.message;
        packet.message = nullptr;
    }

    packet.dataSize = 0;
    packet.bodyType = NONE;
    bodySize = 0;
}

// Setters
void PktDef::SetCmd(CmdType cmd)
{
    FlagUnion flags{};
    flags.byte = packet.header.Flags;

    // Clear command bits
    flags.bits.Drive = 0;
    flags.bits.Status = 0;
    flags.bits.Sleep = 0;

    // Ack must be cleared for commands
    flags.bits.Ack = 0;

    switch (cmd)
    {
    case DRIVE:
        flags.bits.Drive = 1;
        break;

    case SLEEP:
        flags.bits.Sleep = 1;
        break;

    case RESPONSE:   // Telemetry Request
        flags.bits.Status = 1;
        break;
    }

    packet.header.Flags = flags.byte;

    // Reset body
    ClearBody();
}

void PktDef::SetBodyData(char* data, int size)
{
    ClearBody();

    if (data == nullptr || size <= 0)
        return;

    packet.message = new char[size + 1];
    memcpy(packet.message, data, size);
    packet.message[size] = '\0';

    packet.dataSize = size;
    packet.bodyType = MESSAGE_BODY;
    bodySize = size;
}

void PktDef::SetPktCount(int count)
{
    packet.header.PktCount = static_cast<unsigned short>(count);
}

void PktDef::SetStatus(bool enable)
{
    FlagUnion flags{};
    flags.byte = packet.header.Flags;

    flags.bits.Status = enable ? 1 : 0;

    packet.header.Flags = flags.byte;
}

void PktDef::SetDriveBody(const DriveBody& body)
{
    ClearBody();
    SetCmd(DRIVE);
    packet.body.drive = body;
    packet.bodyType = DRIVE_BODY;
    bodySize = sizeof(DriveBody);
    packet.dataSize = bodySize;
}

void PktDef::SetTurnBody(const TurnBody& body)
{
    ClearBody();
    SetCmd(DRIVE);
    packet.body.turn = body;
    packet.bodyType = TURN_BODY;
    bodySize = sizeof(TurnBody);
    packet.dataSize = bodySize;
}

void PktDef::SetAck(bool enable)
{
    FlagUnion flags{};
    flags.byte = packet.header.Flags;
    flags.bits.Ack = enable ? 1 : 0;
    packet.header.Flags = flags.byte;
}

void PktDef::SetTelemetryBody(const TelemetryBody& body)
{
    ClearBody();

    packet.body.telemetry = body;
    packet.bodyType = TELEMETRY_BODY;
    bodySize = sizeof(TelemetryBody);
    packet.dataSize = bodySize;
}

//Getters
CmdType PktDef::GetCmd() const
{
    FlagUnion flags{};
    flags.byte = packet.header.Flags;

    if (flags.bits.Drive)  return DRIVE;
    if (flags.bits.Sleep)  return SLEEP;
    return RESPONSE; // Status bit
}

bool PktDef::GetAck() const
{
    FlagUnion flags{};
    flags.byte = packet.header.Flags;
    return flags.bits.Ack == 1;
}

int PktDef::GetLength() const
{
    return packet.header.Length;
}

char* PktDef::GetBodyData(int& size) const
{
    if (packet.bodyType == MESSAGE_BODY)
    {
        size = packet.dataSize;
        return packet.message;
    }

    size = 0;
    return nullptr;
}

int PktDef::GetPktCount() const
{
    return packet.header.PktCount;
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
    return packet.body.drive;
}

TurnBody PktDef::GetTurnBody() const
{
    return packet.body.turn;
}

TelemetryBody PktDef::GetTelemetry() const
{
    return packet.body.telemetry;
}

char* PktDef::GetRawBuffer() const
{
    return RawBuffer;
}
