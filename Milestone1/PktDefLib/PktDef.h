#pragma once

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


// ===============================
// MAIN CLASS
// ===============================

class PktDef
{
public:
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


    // ===========================
    // GET FUNCTIONS
    // ===========================

    CmdType GetCmd();
    bool GetAck();
    int GetLength();
    char* GetBodyData();
    int GetPktCount();
   
    // ===========================
    // CRC FUNCTIONS
    // ===========================

    bool CheckCRC(char* buffer, int size);
    void CalcCRC();


    // ===========================
    // PACKET GENERATION
    // ===========================

    char* GenPacket();


private:

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


    // ===========================
    // BIT MASKS FOR FLAGS
    // ===========================

    // These help with bit manipulation
    static const unsigned char DRIVE_MASK = 0b10000000; // bit 7
    static const unsigned char STATUS_MASK = 0b01000000; // bit 6
    static const unsigned char SLEEP_MASK = 0b00100000; // bit 5
    static const unsigned char ACK_MASK = 0b00010000; // bit 4
};