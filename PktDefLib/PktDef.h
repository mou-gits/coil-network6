#pragma once
#include <cstddef>

// ---------------------------------------------------------
//  Protocol constants
// ---------------------------------------------------------

// HEADERSIZE: size of Header in bytes (must be calculated by hand)
// Header = PktCount(2) + Flags(1) + Length(1) = 4 bytes
static const int HEADERSIZE = 4;

// Telemetry ACK rule:
// If true, telemetry responses must have ACK = 0.
// If false, ACK is ignored for telemetry.
static const bool ENFORCE_TELEMETRY_ACK_ZERO = false;

// ---------------------------------------------------------
//  Direction constants (spec requires integer definitions)
// ---------------------------------------------------------
enum Direction : unsigned char {
    FORWARD = 1,
    BACKWARD = 2,
    RIGHT = 3,
    LEFT = 4
};

// ---------------------------------------------------------
//  Command type (spec requires DRIVE, SLEEP, RESPONSE)
// ---------------------------------------------------------
enum CmdType {
    DRIVE,
    SLEEP,
    RESPONSE
};

// ---------------------------------------------------------
//  Header (spec requires this exact structure)
// ---------------------------------------------------------
struct Header {
    unsigned short PktCount;
    unsigned char  Flags;
    unsigned char  Length;
};

// ---------------------------------------------------------
//  Flag interpretation helper
// ---------------------------------------------------------
struct FlagBits {
    unsigned char Drive : 1;
    unsigned char Status : 1;
    unsigned char Sleep : 1;
    unsigned char Ack : 1;
    unsigned char Pad : 4;
};

union FlagUnion {
    unsigned char byte;
    FlagBits bits;
};

// ---------------------------------------------------------
//  Fixed-size body structures
// ---------------------------------------------------------
struct DriveBody {
    unsigned char Direction;
    unsigned char Duration;
    unsigned char Power;
};

struct TurnBody {
    unsigned char Direction;
    unsigned short Duration;
};

struct TelemetryBody {
    unsigned short LastPktCounter;
    unsigned short CurrentGrade;
    unsigned short HitCount;
    unsigned short Heading;
    unsigned char  LastCmd;
    unsigned char  LastCmdValue;
    unsigned char  LastCmdPower;
};

// ---------------------------------------------------------
//  Body type selector
// ---------------------------------------------------------
enum BodyType {
    NONE,
    DRIVE_BODY,
    TURN_BODY,
    TELEMETRY_BODY,
    MESSAGE_BODY
};

// ---------------------------------------------------------
//  Union for fixed-size bodies
// ---------------------------------------------------------
union BodyUnion {
    DriveBody     drive;
    TurnBody      turn;
    TelemetryBody telemetry;
};

class PktDef
{
public:
    // ---------------------------------------------------------
    //  Required constructors
    // ---------------------------------------------------------

    // Default constructor:
    // "places the PktDef object in a safe state":
    // - Header fields = 0
    // - Data pointer = nullptr
    // - CRC = 0
    PktDef();

    // Overloaded constructor:
    // "takes a RAW data buffer, parses the data and populates
    //  the Header, Body, and CRC contents of the PktDef object."
    PktDef(char* raw);

    ~PktDef();

    // ---------------------------------------------------------
    //  Required setters
    // ---------------------------------------------------------

    // SetCmd:
    // "sets the packet's command flag based on the CmdType"
    void SetCmd(CmdType cmd);

    // SetBodyData:
    // "allocates the packet's Body field and copies the provided
    //  RAW data buffer into the object's buffer"
    void SetBodyData(char* data, int size);

    // SetPktCount:
    // "sets the object's PktCount header variable"
    void SetPktCount(int count);

    void SetStatus(bool enable);
    void SetDriveBody(const DriveBody& body);
    void SetTurnBody(const TurnBody& body);
    void SetAck(bool enable);

    // ---------------------------------------------------------
    //  Required getters
    // ---------------------------------------------------------

    // GetCmd:
    // "returns the CmdType based on the set command flag bit"
    CmdType GetCmd() const;

    // GetAck:
    // "returns True/False based on the Ack flag in the header"
    bool GetAck() const;

    // GetLength:
    // "returns the packet Length in bytes"
    int GetLength() const;

    // GetBodyData:
    // "returns a pointer to the object's Body field"
    char* GetBodyData() const;

    // GetPktCount:
    // "returns the PktCount value"
    int GetPktCount() const;

    unsigned char GetFlags() const;
    int GetBodySize() const;
    DriveBody GetDriveBody() const;
    TurnBody GetTurnBody() const;
    TelemetryBody GetTelemetry() const;
    char* GetRawBuffer() const;

    // ---------------------------------------------------------
    //  Required CRC functions
    // ---------------------------------------------------------

    // CheckCRC:
    // "takes a RAW buffer and size, calculates CRC, and returns
    //  TRUE if it matches the packet's CRC, otherwise FALSE"
    bool CheckCRC(char* buffer, int size);

    // CalcCRC:
    // "calculates the CRC and sets the object's packet CRC"
    void CalcCRC();

    // ---------------------------------------------------------
    //  Required serialization
    // ---------------------------------------------------------

    // GenPacket:
    // "allocates RawBuffer and transfers the object's member
    //  variables into a RAW data packet; returns RawBuffer"
    char* GenPacket();

    // ---------------------------------------------------------
    //  VALIDATORS 
    // ---------------------------------------------------------
    bool IsValidDirection(Direction dir) const;
    bool IsValid() const;
    bool IsDriveCommand() const;
    bool IsTurnCommand() const;
    bool IsTelemetryRequest() const;
    bool IsAckResponse() const;
    bool IsTelemetryResponse() const;

private:
    // ---------------------------------------------------------
    //  CmdPacket (spec requires this struct)
    // ---------------------------------------------------------
        struct CmdPacket {
            Header    header;
            BodyType  bodyType;
            BodyUnion body;
            char* Data;      // variable-length body (message)
            int       dataSize;
            char      CRC;
        };
    CmdPacket packet;
    char* RawBuffer;

    void ClearBody();

    bool validPacket;
    int  bodySize;
};