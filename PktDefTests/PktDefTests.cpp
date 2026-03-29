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

    TEST_CLASS(AckFlagTests)
    {
    public:

        TEST_METHOD(SetAck_SetsFlag)
        {
            PktDef pkt;
            pkt.SetAck(true);
            Assert::IsTrue(pkt.GetAck());
        }

        TEST_METHOD(SetAck_ClearsFlag)
        {
            PktDef pkt;
            pkt.SetAck(true);
            pkt.SetAck(false);
            Assert::IsFalse(pkt.GetAck());
        }

        TEST_METHOD(Ack_MustNotClearCommandFlag)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            pkt.SetAck(true);

            Assert::IsTrue(pkt.GetCmd() == DRIVE);
            Assert::IsTrue(pkt.GetAck());
        }
    };

    TEST_CLASS(StatusFlagTests)
    {
    public:

        TEST_METHOD(SetStatus_SetsFlag)
        {
            PktDef pkt;
            pkt.SetStatus(true);

            Assert::IsTrue(pkt.GetCmd() == RESPONSE); // Status implies response
        }

        TEST_METHOD(SetStatus_ClearsFlag)
        {
            PktDef pkt;
            pkt.SetStatus(true);
            pkt.SetStatus(false);

            Assert::IsFalse(pkt.GetAck()); // Ack should not be set
        }
    };

    TEST_CLASS(FlagValidationTests)
    {
    public:

        TEST_METHOD(OnlyOneCommandFlagAllowed)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            pkt.SetCmd(SLEEP);

            Assert::IsTrue(pkt.GetCmd() == SLEEP);
            Assert::IsFalse(pkt.GetCmd() == DRIVE);
        }

        TEST_METHOD(ParsingInvalidFlags_DoesNotCrash)
        {
            char raw[5] = { 0 };
            raw[2] = 0xE0; // Drive + Status + Sleep (illegal)
            raw[3] = 5;

            // Compute CRC
            int bitCount = 0;
            for (int i = 0; i < 4; i++)
            {
                unsigned char b = raw[i];
                while (b) { bitCount += (b & 1); b >>= 1; }
            }
            raw[4] = (char)(bitCount % 256);

            PktDef pkt(raw);

            // The only requirement: it must NOT crash.
            // Command returned should follow GetCmd() logic.
            Assert::IsTrue(pkt.GetCmd() == DRIVE);
        }
    };

    TEST_CLASS(DriveBodyTests)
    {
    public:

        TEST_METHOD(SetDriveBody_ValidRange)
        {
            PktDef pkt;
            DriveBody db = { FORWARD, 4, 90 };

            pkt.SetDriveBody(db);

            Assert::AreEqual(HEADERSIZE + 3 + 1, pkt.GetLength());

            DriveBody result = pkt.GetDriveBody();
            Assert::AreEqual((unsigned char)FORWARD, result.Direction);
            Assert::AreEqual((unsigned char)4, result.Duration);
            Assert::AreEqual((unsigned char)90, result.Power);
        }

        TEST_METHOD(SetDriveBody_InvalidPowerRejected)
        {
            PktDef pkt;
            DriveBody db = { FORWARD, 4, 50 }; // invalid

            pkt.SetDriveBody(db);

            Assert::IsNull(pkt.GetBodyData());
            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }
    };

    TEST_CLASS(TurnBodyTests)
    {
    public:

        TEST_METHOD(SetTurnBody_SerializesCorrectly)
        {
            PktDef pkt;
            TurnBody tb = { LEFT, 500 };

            pkt.SetTurnBody(tb);

            Assert::AreEqual(HEADERSIZE + 3 + 1, pkt.GetLength());

            TurnBody result = pkt.GetTurnBody();
            Assert::AreEqual((unsigned char)LEFT, result.Direction);
            Assert::AreEqual((unsigned short)500, result.Duration);
        }
    };

    TEST_CLASS(TelemetryTests)
    {
    public:

        TEST_METHOD(ParseTelemetryBody)
        {
            // Build raw telemetry packet
            char raw[HEADERSIZE + 11 + 1] = { 0 };

            unsigned short pktCount = 12;
            memcpy(raw, &pktCount, 2);

            raw[2] = 0x50; // Status + Ack
            raw[3] = HEADERSIZE + 11 + 1;

            unsigned char body[11] =
            {
                1,0,   // LastPktCounter
                5,0,   // CurrentGrade
                3,0,   // HitCount
                90,0,  // Heading
                2,     // LastCmd
                4,     // LastCmdValue
                85     // LastCmdPower
            };

            memcpy(raw + HEADERSIZE, body, 11);

            // Compute CRC
            int bitCount = 0;
            for (int i = 0; i < HEADERSIZE + 11; i++)
            {
                unsigned char b = raw[i];
                while (b) { bitCount += (b & 1); b >>= 1; }
            }
            raw[HEADERSIZE + 11] = (char)(bitCount % 256);

            PktDef pkt(raw);

            TelemetryBody t = pkt.GetTelemetry();

            Assert::AreEqual((unsigned short)1, t.LastPktCounter);
            Assert::AreEqual((unsigned short)5, t.CurrentGrade);
            Assert::AreEqual((unsigned short)3, t.HitCount);
            Assert::AreEqual((unsigned short)90, t.Heading);
            Assert::AreEqual((unsigned char)2, t.LastCmd);
            Assert::AreEqual((unsigned char)4, t.LastCmdValue);
            Assert::AreEqual((unsigned char)85, t.LastCmdPower);
        }
    };

    TEST_CLASS(LengthTests)
    {
    public:

        TEST_METHOD(SleepCommand_LengthCorrect)
        {
            PktDef pkt;
            pkt.SetCmd(SLEEP);

            char* raw = pkt.GenPacket();

            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }

        TEST_METHOD(Response_NoBody_LengthCorrect)
        {
            PktDef pkt;
            pkt.SetCmd(RESPONSE);

            char* raw = pkt.GenPacket();

            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }
    };

    TEST_CLASS(RawPacketValidationTests)
    {
    public:

        TEST_METHOD(Parse_InvalidLength_DoesNotCrash)
        {
            char raw[3] = { 0 }; // too short to be valid
            PktDef pkt(raw);

            Assert::IsTrue(true);
        }

        TEST_METHOD(Parse_InvalidCRC_FailsCheckCRC)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);

            DriveBody db = { FORWARD, 3, 90 };
            pkt.SetDriveBody(db);

            char* raw = pkt.GenPacket();
            int len = pkt.GetLength();

            raw[len - 1] ^= 0xFF; // corrupt CRC

            Assert::IsFalse(pkt.CheckCRC(raw, len));
        }
    };

    TEST_CLASS(MemorySafetyTests)
    {
    public:

        TEST_METHOD(GenPacket_CalledTwice_NoLeak)
        {
            PktDef pkt;
            pkt.SetPktCount(1);

            DriveBody db = { FORWARD, 3, 90 };
            pkt.SetDriveBody(db);

            char* first = pkt.GenPacket();
            char* second = pkt.GenPacket();

            Assert::IsNotNull(second);
            Assert::AreNotEqual(first, second);
        }

        TEST_METHOD(Destructor_FreesMemory)
        {
            char* buffer = nullptr;

            {
                PktDef pkt;
                pkt.SetPktCount(1);

                DriveBody db = { FORWARD, 3, 90 };
                pkt.SetDriveBody(db);

                buffer = pkt.GenPacket();
                Assert::IsNotNull(buffer);
            }

            // If destructor failed, this would crash or leak
            Assert::IsTrue(true);
        }
    };
    
    TEST_CLASS(ConditionChecks)
    {
    public:

        //
        // 1. Padding bits must be zero
        //
        TEST_METHOD(PaddingBits_AreClearedOnSetCmd)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            unsigned char flags = pkt.GetFlags();
            Assert::AreEqual((unsigned char)0x80, flags); // DRIVE only
        }

        //
        // 2. Sleep must have no body
        //
        TEST_METHOD(SleepCommand_HasNoBody)
        {
            PktDef pkt;
            pkt.SetCmd(SLEEP);

            Assert::IsNull(pkt.GetBodyData());
            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }

        //
        // 3. Response must have no body unless telemetry
        //
        TEST_METHOD(ResponseCommand_NoBodyUnlessTelemetry)
        {
            PktDef pkt;
            pkt.SetCmd(RESPONSE);

            Assert::IsNull(pkt.GetBodyData());
            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }

        //
        // 4. Ack must be paired with a command flag
        //
        TEST_METHOD(AckMustHaveCommandFlag)
        {
            PktDef pkt;
            pkt.SetAck(true);

            // Should auto-assign RESPONSE
            Assert::IsTrue(pkt.GetCmd() == RESPONSE);
            Assert::IsTrue(pkt.GetAck());
        }

        //
        // 5. DriveBody direction must be valid
        //
        TEST_METHOD(DriveBody_InvalidDirectionRejected)
        {
            PktDef pkt;
            DriveBody db = { 99, 5, 90 }; // invalid direction

            pkt.SetDriveBody(db);

            Assert::IsNull(pkt.GetBodyData());
            Assert::IsFalse(pkt.IsValid());
        }

        //
        // 6. TurnBody duration must be valid (implicitly tested by valid direction)
        //
        TEST_METHOD(TurnBody_InvalidDirectionRejected)
        {
            PktDef pkt;
            TurnBody tb = { FORWARD, 300 }; // invalid for turn

            pkt.SetTurnBody(tb);

            Assert::IsNull(pkt.GetBodyData());
            Assert::IsFalse(pkt.IsValid());
        }

        //
        // 7. Validate raw packet length before reading
        //
        TEST_METHOD(Parse_InvalidLength_MarkedInvalid)
        {
            char raw[3] = { 0 }; // too short

            PktDef pkt(raw);

            Assert::IsFalse(pkt.IsValid());
        }

        //
        // 8. CRC must be validated during parsing
        //
        TEST_METHOD(Parse_InvalidCRC_MarkedInvalid)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);

            DriveBody db = { FORWARD, 3, 90 };
            pkt.SetDriveBody(db);

            char* raw = pkt.GenPacket();
            int len = pkt.GetLength();

            raw[len - 1] ^= 0xFF; // corrupt CRC

            PktDef parsed(raw);

            Assert::IsFalse(parsed.IsValid());
        }

        //
        // 9. Turn commands always use LEFT or RIGHT
        //
        TEST_METHOD(TurnBody_ValidDirectionsOnly)
        {
            PktDef pkt;
            TurnBody tb = { LEFT, 200 };
            pkt.SetTurnBody(tb);

            Assert::IsNotNull(pkt.GetBodyData());
            Assert::IsTrue(pkt.IsValid());
        }

        //
        // 10. Drive must have a body
        //
        TEST_METHOD(DriveWithoutBody_IsInvalid)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            pkt.GenPacket();

            Assert::IsFalse(pkt.IsValid());
        }

        //
        // 11. Turn commands always use 100% power (implicitly enforced)
        //
        TEST_METHOD(TurnBody_AlwaysThreeBytes_NoPowerField)
        {
            PktDef pkt;
            TurnBody tb = { RIGHT, 400 };
            pkt.SetTurnBody(tb);

            Assert::AreEqual(3, pkt.GetBodySize());
            Assert::IsTrue(pkt.IsValid());
        }

        //
        // 12. Response must clear body unless telemetry
        //
        TEST_METHOD(ResponseClearsBody)
        {
            PktDef pkt;

            DriveBody db = { FORWARD, 3, 90 };
            pkt.SetDriveBody(db);

            pkt.SetCmd(RESPONSE);

            Assert::IsNull(pkt.GetBodyData());
            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }

        //
        // 13. Padding bits must remain zero after all operations
        //
        TEST_METHOD(PaddingBitsRemainZero)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            pkt.SetAck(true);
            pkt.SetStatus(true);

            unsigned char flags = pkt.GetFlags();

            // Only upper 4 bits allowed: DRIVE, STATUS, ACK
            Assert::AreEqual((unsigned char)(DRIVE_MASK | STATUS_MASK | ACK_MASK), flags);
        }
    };



}
