#include "pktdef.h"

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

PktDef::PktDef(char* rawData): RawBuffer(nullptr), bodySize(0)
{
    // Initialize packet.header
    packet.header.PktCount = 0;
    packet.header.Flags = 0;
    packet.header.Length = 0;

    // Initialize packet.Data
    packet.Data = nullptr;

    // Initialize packet.CRC
    packet.CRC = 0;

    // 'rawData' is currently unused; suppress unused parameter warning
    (void)rawData;
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
        packet.header.Flags |= DRIVE_MASK;
        break;

    case SLEEP:
        packet.header.Flags |= SLEEP_MASK;
        break;

    case RESPONSE:
        packet.header.Flags |= STATUS_MASK;
        break;

    default:
        break;
    }
}

void PktDef::SetBodyData(char* data, int size)
{
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
    return false;
}

void PktDef::CalcCRC()
{
}

char* PktDef::GenPacket()
{
    return nullptr;
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