#include "pch.h"
#include "CppUnitTest.h"
#include "../PktDefLib/PktDef.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PktDefTests
{
    TEST_CLASS(DefaultConstructorTests)
    {
    public:
        TEST_METHOD(DefaultCtor_PktCountIsZero)
        {
            PktDef pkt;
            Assert::AreEqual(0, pkt.GetPktCount());
        }

        TEST_METHOD(DefaultCtor_LengthIsZero)
        {
            PktDef pkt;
            Assert::AreEqual(0, pkt.GetLength());
        }

        TEST_METHOD(DefaultCtor_BodyIsNull)
        {
            PktDef pkt;
            Assert::IsNull(pkt.GetBodyData());
        }

        TEST_METHOD(DefaultCtor_AckIsFalse)
        {
            PktDef pkt;
            Assert::IsFalse(pkt.GetAck());
        }

        TEST_METHOD(DefaultCtor_CmdIsResponse)
        {
            PktDef pkt;
            Assert::IsTrue(pkt.GetCmd() == RESPONSE);
        }
    };

    TEST_CLASS(SetGetCmdTests)
    {
    public:
        TEST_METHOD(SetCmd_Drive)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            Assert::IsTrue(pkt.GetCmd() == DRIVE);
        }

        TEST_METHOD(SetCmd_Sleep)
        {
            PktDef pkt;
            pkt.SetCmd(SLEEP);
            Assert::IsTrue(pkt.GetCmd() == SLEEP);
        }

        TEST_METHOD(SetCmd_Response)
        {
            PktDef pkt;
            pkt.SetCmd(RESPONSE);
            Assert::IsTrue(pkt.GetCmd() == RESPONSE);
        }

        TEST_METHOD(SetCmd_OverwritesPrevious)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            pkt.SetCmd(SLEEP);
            Assert::IsTrue(pkt.GetCmd() == SLEEP);
        }
    };

    TEST_CLASS(PktCountTests)
    {
    public:
        TEST_METHOD(SetPktCount_GetReturnsValue)
        {
            PktDef pkt;
            pkt.SetPktCount(42);
            Assert::AreEqual(42, pkt.GetPktCount());
        }

        TEST_METHOD(SetPktCount_LargeValue)
        {
            PktDef pkt;
            pkt.SetPktCount(65535);
            Assert::AreEqual(65535, pkt.GetPktCount());
        }
    };

    TEST_CLASS(BodyDataTests)
    {
    public:
        TEST_METHOD(SetBodyData_DriveBody)
        {
            PktDef pkt;
            DriveBody db;
            db.Direction = FORWARD;
            db.Duration = 5;
            db.Power = 90;

            pkt.SetBodyData((char*)&db, sizeof(DriveBody));

            Assert::IsNotNull(pkt.GetBodyData());
            Assert::AreEqual(HEADERSIZE + (int)sizeof(DriveBody) + 1, pkt.GetLength());

            DriveBody* result = (DriveBody*)pkt.GetBodyData();
            Assert::AreEqual((unsigned char)FORWARD, result->Direction);
            Assert::AreEqual((unsigned char)5, result->Duration);
            Assert::AreEqual((unsigned char)90, result->Power);
        }

        TEST_METHOD(SetBodyData_TurnBody)
        {
            PktDef pkt;
            TurnBody tb;
            tb.Direction = LEFT;
            tb.Duration = 300;

            pkt.SetBodyData((char*)&tb, sizeof(TurnBody));

            Assert::IsNotNull(pkt.GetBodyData());
            Assert::AreEqual(HEADERSIZE + (int)sizeof(TurnBody) + 1, pkt.GetLength());

            TurnBody* result = (TurnBody*)pkt.GetBodyData();
            Assert::AreEqual((unsigned char)LEFT, result->Direction);
            Assert::AreEqual((unsigned short)300, result->Duration);
        }

        TEST_METHOD(SetBodyData_UpdatesLength)
        {
            PktDef pkt;
            DriveBody db = { BACKWARD, 3, 80 };
            pkt.SetBodyData((char*)&db, sizeof(DriveBody));
            Assert::AreEqual(8, pkt.GetLength());
        }

        TEST_METHOD(SetBodyData_NullGuard)
        {
            PktDef pkt;
            pkt.SetBodyData(nullptr, 0);
            Assert::IsNull(pkt.GetBodyData());
        }

        TEST_METHOD(SetBodyData_ReplacesOldBody)
        {
            PktDef pkt;
            char firstData[3] = { 1, 5, 80 };
            char secondData[2] = { 4, 9 };

            pkt.SetBodyData(firstData, 3);
            pkt.SetBodyData(secondData, 2);

            Assert::IsNotNull(pkt.GetBodyData());
            Assert::AreEqual(7, pkt.GetLength());
            Assert::AreEqual((char)4, pkt.GetBodyData()[0]);
            Assert::AreEqual((char)9, pkt.GetBodyData()[1]);
        }
    };

    TEST_CLASS(CRCTests)
    {
    public:
        TEST_METHOD(CalcCRC_CheckCRC_Match)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);
            DriveBody db = { FORWARD, 5, 90 };
            pkt.SetBodyData((char*)&db, sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            int len = pkt.GetLength();

            Assert::IsTrue(pkt.CheckCRC(raw, len));
        }

        TEST_METHOD(CheckCRC_CorruptedBuffer_Fails)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);
            DriveBody db = { FORWARD, 5, 90 };
            pkt.SetBodyData((char*)&db, sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            int len = pkt.GetLength();

            raw[HEADERSIZE] ^= 0xFF;

            Assert::IsFalse(pkt.CheckCRC(raw, len));
        }

        TEST_METHOD(CalcCRC_SleepNoBody)
        {
            PktDef pkt;
            pkt.SetPktCount(10);
            pkt.SetCmd(SLEEP);

            char* raw = pkt.GenPacket();
            int len = pkt.GetLength();

            Assert::IsTrue(pkt.CheckCRC(raw, len));
        }
    };

    TEST_CLASS(GenPacketTests)
    {
    public:
        TEST_METHOD(GenPacket_DriveRoundTrip)
        {
            PktDef original;
            original.SetPktCount(7);
            original.SetCmd(DRIVE);
            DriveBody db = { BACKWARD, 3, 85 };
            original.SetBodyData((char*)&db, sizeof(DriveBody));

            char* raw = original.GenPacket();
            int len = original.GetLength();

            PktDef parsed(raw);

            Assert::AreEqual(7, parsed.GetPktCount());
            Assert::IsTrue(parsed.GetCmd() == DRIVE);
            Assert::AreEqual(len, parsed.GetLength());

            DriveBody* result = (DriveBody*)parsed.GetBodyData();
            Assert::IsNotNull(result);
            Assert::AreEqual((unsigned char)BACKWARD, result->Direction);
            Assert::AreEqual((unsigned char)3, result->Duration);
            Assert::AreEqual((unsigned char)85, result->Power);

            Assert::IsTrue(parsed.CheckCRC(raw, len));
        }

        TEST_METHOD(GenPacket_TurnRoundTrip)
        {
            PktDef original;
            original.SetPktCount(15);
            original.SetCmd(DRIVE);
            TurnBody tb = { RIGHT, 500 };
            original.SetBodyData((char*)&tb, sizeof(TurnBody));

            char* raw = original.GenPacket();
            int len = original.GetLength();

            PktDef parsed(raw);

            Assert::AreEqual(15, parsed.GetPktCount());
            Assert::IsTrue(parsed.GetCmd() == DRIVE);

            TurnBody* result = (TurnBody*)parsed.GetBodyData();
            Assert::IsNotNull(result);
            Assert::AreEqual((unsigned char)RIGHT, result->Direction);
            Assert::AreEqual((unsigned short)500, result->Duration);

            Assert::IsTrue(parsed.CheckCRC(raw, len));
        }

        TEST_METHOD(GenPacket_SleepRoundTrip)
        {
            PktDef original;
            original.SetPktCount(20);
            original.SetCmd(SLEEP);

            char* raw = original.GenPacket();
            int len = original.GetLength();

            PktDef parsed(raw);

            Assert::AreEqual(20, parsed.GetPktCount());
            Assert::IsTrue(parsed.GetCmd() == SLEEP);
            Assert::IsNull(parsed.GetBodyData());
            Assert::IsTrue(parsed.CheckCRC(raw, len));
        }

        TEST_METHOD(GenPacket_ResponseRoundTrip)
        {
            PktDef original;
            original.SetPktCount(99);
            original.SetCmd(RESPONSE);

            char* raw = original.GenPacket();
            int len = original.GetLength();

            PktDef parsed(raw);

            Assert::AreEqual(99, parsed.GetPktCount());
            Assert::IsTrue(parsed.GetCmd() == RESPONSE);
            Assert::IsTrue(parsed.CheckCRC(raw, len));
        }
    };

    TEST_CLASS(AckTests)
    {
    public:
        TEST_METHOD(Ack_ParsedFromRawBuffer)
        {
            // Manually build a raw ACK packet (Drive + Ack, no body)
            // Header: PktCount=1, Flags=Drive(0x80)+Ack(0x10)=0x90, Length=5
            char raw[5];
            memset(raw, 0, 5);

            unsigned short pktCount = 1;
            memcpy(raw, &pktCount, 2);
            raw[2] = (char)0x90; // Drive=bit7, Ack=bit4
            raw[3] = 5;          // Length

            // Calculate CRC: count bits in first 4 bytes
            int bitCount = 0;
            for (int i = 0; i < 4; i++) {
                unsigned char b = (unsigned char)raw[i];
                while (b) { bitCount += (b & 1); b >>= 1; }
            }
            raw[4] = (char)(bitCount % 256);

            PktDef pkt(raw);
            Assert::IsTrue(pkt.GetAck());
            Assert::IsTrue(pkt.GetCmd() == DRIVE);
        }
    };
}
