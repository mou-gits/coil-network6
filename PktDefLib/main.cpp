#include <iostream>
#include "PktDef.h"

using namespace std;

void PrintPacket(const PktDef& pkt)
{
    cout << "Packet Count: " << pkt.GetPktCount() << endl;
    cout << "Flags: 0x" << hex << (int)pkt.GetFlags() << dec << endl;
    cout << "Length: " << pkt.GetLength() << endl;
    cout << "Valid: " << (pkt.IsValid() ? "YES" : "NO") << endl;

    if (pkt.GetBodyData() != nullptr)
    {
        cout << "Body Size: " << pkt.GetBodySize() << endl;
        cout << "Body Bytes: ";
        for (int i = 0; i < pkt.GetBodySize(); i++)
        {
            cout << (int)(unsigned char)pkt.GetBodyData()[i] << " ";
        }
        cout << endl;
    }
    else
    {
        cout << "No Body" << endl;
    }

    cout << "-----------------------------" << endl;
}

int main()
{
    cout << "PktDef Demo Program" << endl;
    cout << "-----------------------------" << endl;

    // 1. Create a Drive command
    PktDef drivePkt;
    drivePkt.SetPktCount(1);
    drivePkt.SetCmd(DRIVE);

    DriveBody db;
    db.Direction = FORWARD;
    db.Duration = 5;
    db.Power = 90;

    drivePkt.SetDriveBody(db);
    drivePkt.GenPacket();

    cout << "Drive Packet:" << endl;
    PrintPacket(drivePkt);

    // 2. Create a Turn command
    PktDef turnPkt;
    turnPkt.SetPktCount(2);
    turnPkt.SetCmd(DRIVE);

    TurnBody tb;
    tb.Direction = LEFT;
    tb.Duration = 300;

    turnPkt.SetTurnBody(tb);
    turnPkt.GenPacket();

    cout << "Turn Packet:" << endl;
    PrintPacket(turnPkt);

    // 3. Parse a raw packet
    cout << "Parsing Raw Packet:" << endl;
    char* raw = drivePkt.GetRawBuffer();
    PktDef parsed(raw);

    PrintPacket(parsed);

    cout << "Demo complete." << endl;
    return 0;
}
