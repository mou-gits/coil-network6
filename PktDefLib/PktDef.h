#pragma once

#define ALL_RIGHT_BITS 0x00FF

// Flag bit masks
#define DRIVE_MASK   0x80   // bit 7
#define STATUS_MASK  0x40   // bit 6
#define SLEEP_MASK   0x20   // bit 5
#define ACK_MASK     0x10   // bit 4

// Lower 4 bits are padding and must be zero
#define PADDING_MASK 0x0F


// ===============================
// CONSTANTS (from assignment)
// ===============================

const int FORWARD = 1;
const int BACKWARD = 2;
const int RIGHT = 3;
const int LEFT = 4;

// Header size: PktCount (2 bytes) + Flags (1 byte) + Length (1 byte)
const int HEADERSIZE = 4;


// ===============================
// ENUM FOR COMMAND TYPE
// ===============================
enum CmdType
{
    DRIVE,
    SLEEP,
    RESPONSE
};



// ===============================
// STRUCTURES
// ===============================

// Header structure (logical representation)
struct Header
{
    unsigned short PktCount;   // 2 bytes

    // Instead of storing flags separately,
    // we will store them in ONE byte using bit manipulation
    unsigned char Flags;       // 1 byte (Drive, Status, Sleep, Ack, padding)

    unsigned char Length;      // 1 byte (total packet length)
};


// Drive body (forward/backward)
struct DriveBody
{
    unsigned char Direction;
    unsigned char Duration;
    unsigned char Power;
};


// Turn body (left/right)
struct TurnBody
{
    unsigned char Direction;
    unsigned short Duration;
};

struct TelemetryBody
{
    unsigned short LastPktCounter;
    unsigned short CurrentGrade;
    unsigned short HitCount;
    unsigned short Heading;
    unsigned char LastCmd;
    unsigned char LastCmdValue;
    unsigned char LastCmdPower;
};


// ===============================
// MAIN CLASS
// ===============================

class PktDef
{
public:

    bool IsValid() const;             // marks packet validity after parsing

    // ===========================
    // CONSTRUCTORS / DESTRUCTOR
    // ===========================

    PktDef();                  // default constructor
    PktDef(char* rawData);     // construct from raw packet
    ~PktDef();                 // destructor (important for memory cleanup)


    // ===========================
    // SET FUNCTIONS
    // ===========================

    void SetCmd(CmdType cmd);
    void SetBodyData(char* data, int size);
    void SetPktCount(int count);
    void SetAck(bool enable);
    void SetStatus(bool enable);

    void SetDriveBody(const DriveBody& body);
    void SetTurnBody(const TurnBody& body);


    // ===========================
    // GET FUNCTIONS
    // ===========================

    int GetPktCount() const;
    int GetLength() const;
    char* GetBodyData() const;
    bool GetAck() const;
    CmdType GetCmd() const;

    DriveBody GetDriveBody() const;
    TurnBody GetTurnBody() const;
    TelemetryBody GetTelemetry() const;

    char* GetRawBuffer() const;

    unsigned char GetFlags() const;
    int GetBodySize() const;

    // ===========================
    // CRC FUNCTIONS
    // ===========================

    bool CheckCRC(char* buffer, int size);
    void CalcCRC();


    // ===========================
    // PACKET GENERATION
    // ===========================

    char* GenPacket();

    /*
    -------------------------------------------------------------------------
    Protocol Architecture Note – Telemetry vs. ACK Interpretation
    -------------------------------------------------------------------------

    The robot protocol defines three distinct situations involving the Status
    bit, but the specification is ambiguous about the ACK bit in one of them.
    To avoid misinterpretation and ensure deterministic packet validation,
    this class explicitly separates the three cases:

        Situation #1 – Telemetry Request (Computer ? Robot)
            A STATUS command sent by the computer to request housekeeping
            telemetry. This is a command packet and therefore MUST NOT set
            the ACK bit. If ACK = 1, the robot will reject the command.

        Situation #2 – Acknowledgement Response (Robot ? Computer)
            After receiving a valid command (Drive, Sleep, or Status), the
            robot sends an ACK packet with both Status = 1 and Ack = 1.
            This packet confirms receipt of the command but does not contain
            telemetry data.

        Situation #3 – Telemetry Response (Robot ? Computer)
            The robot sends a second packet containing the actual telemetry
            data. The protocol explicitly states that Status = 1 must be set,
            but it does NOT specify whether Ack must be 0 or 1. To resolve
            this ambiguity, this implementation introduces a compile-time
            constant (ENFORCE_TELEMETRY_ACK_ZERO). When enabled, telemetry
            responses are required to have Ack = 0; when disabled, the ACK
            bit is ignored for telemetry packets.

    This separation ensures:
        - Commands are validated strictly according to protocol rules.
        - ACK responses are unambiguously identified.
        - Telemetry responses can be validated in either strict or relaxed
          mode depending on project requirements or instructor expectations.

    These rules are enforced by:
        IsTelemetryRequest()   – validates Situation #1
        IsAckResponse()        – validates Situation #2
        IsTelemetryResponse()  – validates Situation #3 (with ACK policy)

    -------------------------------------------------------------------------
*/
    bool IsTelemetryResponse() const;
    bool IsAckResponse() const;
    bool IsTelemetryRequest() const;

private:
    // If true : Telemetry responses MUST have ACK = 0
    // If false: ACK is ignored for telemetry responses
    static const bool ENFORCE_TELEMETRY_ACK_ZERO = true;

    bool validPacket = true;
    bool IsDriveCommand() const;
    bool IsTurnCommand() const;
    void ClearBody();                 // clears Data and resets bodySize
    bool IsValidDirection(unsigned char dir) const;

    // ===========================
    // INTERNAL PACKET STRUCTURE
    // ===========================

    struct CmdPacket
    {
        Header header;   // header info
        char* Data;      // dynamic body
        char CRC;        // 1 byte CRC
    };


    // ===========================
    // PRIVATE DATA MEMBERS
    // ===========================

    CmdPacket packet;    // main structured packet

    char* RawBuffer;     // serialized packet (ready to send)

    int bodySize;        // size of body (important for length calculation)
};