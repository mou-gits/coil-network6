#pragma once
#include <cstddef>

// ---------------------------------------------------------
//  Protocol constants
// ---------------------------------------------------------

static const int HEADERSIZE = 4;   // PktCount(2) + Flags(1) + Length(1)
static const bool ENFORCE_TELEMETRY_ACK_ZERO = false;

// ---------------------------------------------------------
//  Direction constants
// ---------------------------------------------------------
enum Direction : unsigned char {
    FORWARD = 1,
    BACKWARD = 2,
    RIGHT = 3,
    LEFT = 4
};

// ---------------------------------------------------------
//  Command type
// ---------------------------------------------------------
enum CmdType {
    DRIVE,
    SLEEP,
    RESPONSE
};

enum class PacketClass {
    Invalid,

    DriveCommand,
    TurnCommand,
    SleepCommand,
    TelemetryCommand,

    AckResponse,
    NAckResponse,

    TelemetryResponse
};

// ---------------------------------------------------------
//  Header
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
#pragma pack(push, 1)

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
#pragma pack(pop)
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
    //  Constructors
    // ---------------------------------------------------------

    PktDef();          // Safe state
    PktDef(char* raw); // Parse raw packet
    ~PktDef();

    // ---------------------------------------------------------
    //  Setters
    // ---------------------------------------------------------
    void SetCmd(CmdType cmd);
    void SetBodyData(char* data, int size);
    void SetPktCount(int count);

    void SetStatus(bool enable);
    void SetDriveBody(const DriveBody& body);
    void SetTurnBody(const TurnBody& body);
    void SetAck(bool enable);
    void SetTelemetryBody(const TelemetryBody& body);

    // ---------------------------------------------------------
    //  Getters
    // ---------------------------------------------------------
    CmdType GetCmd() const;
    bool GetAck() const;
    int GetLength() const;
    char* GetBodyData(int& size) const;
    int GetPktCount() const;

    unsigned char GetFlags() const;
    int GetBodySize() const;

    DriveBody GetDriveBody() const;
    TurnBody GetTurnBody() const;
    TelemetryBody GetTelemetry() const;

    char* GetRawBuffer() const;


    bool LooksLikeDriveBody() const;
    bool LooksLikeTurnBody() const;
    bool LooksLikeTelemetryBody() const;

    // ---------------------------------------------------------
    //  Serialization
    // ---------------------------------------------------------
    char* GenPacket();

    // ---------------------------------------------------------
    //  CRC
    // ---------------------------------------------------------
    bool CheckCRC(char* buffer, int size) const;
    void CalcCRC();

    // ---------------------------------------------------------
    //  Validators / Classifier
    // ---------------------------------------------------------
    bool IsValidDirection(Direction dir) const;

    PacketClass ClassifyPacket() const;

    bool IsDriveCommand() const;
    bool IsTurnCommand() const;
    bool IsTelemetryCommand() const;
    bool IsSleepCommand() const;

    bool IsAckResponse() const;
    bool IsNAckResponse() const;
    bool IsTelemetryResponse() const;

    bool IsValid() const;

    void ClearBody();

private:

    // ---------------------------------------------------------
    //  CmdPacket (spec-required)
    // ---------------------------------------------------------
    struct CmdPacket {
        Header    header;

        BodyType  bodyType;
        BodyUnion body;

        char* Data;       // Raw body bytes (always raw)
        int   dataSize;

        char* message;    // Null unless MESSAGE_BODY
        char  CRC;
    };

    CmdPacket packet;
    char* RawBuffer;


    int  bodySize;
    bool validPacket;
};
