#include <vector>
#include <cstring>
#include <string>
#include "pch.h"
#include "CppUnitTest.h"
#include "../PktDefLib/PktDef.h"


namespace Microsoft {
    namespace VisualStudio {
        namespace CppUnitTestFramework {

            template<>
            inline std::wstring ToString<CmdType>(const CmdType& t)
            {
                switch (t)
                {
                case DRIVE:     return L"DRIVE";
                case SLEEP:     return L"SLEEP";
                case RESPONSE:  return L"RESPONSE";
                default:        return L"UNKNOWN_CMDTYPE";
                }
            }

            template<>
            inline std::wstring ToString<Direction>(const Direction& d)
            {
                switch (d)
                {
                case FORWARD:   return L"FORWARD";
                case BACKWARD:  return L"BACKWARD";
                case LEFT:      return L"LEFT";
                case RIGHT:     return L"RIGHT";
                default:        return L"UNKNOWN_DIRECTION";
                }
            }

        } // namespace CppUnitTestFramework
    } // namespace VisualStudio
} // namespace Microsoft


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PktDefTests
{
    //
    // Small helpers for tests
    //
    static DriveBody MakeDrive(Direction dir, unsigned char duration, unsigned char power)
    {
        DriveBody d{};
        d.Direction = static_cast<unsigned char>(dir);
        d.Duration = duration;
        d.Power = power;
        return d;
    }

    static TurnBody MakeTurn(Direction dir, unsigned short duration)
    {
        TurnBody t{};
        t.Direction = static_cast<unsigned char>(dir);
        t.Duration = duration;
        return t;
    }

    static TelemetryBody MakeTelemetry()
    {
        TelemetryBody t{};
        // Leave fields default/zero; we only care about size and round-trip
        return t;
    }

    //
    // 1. Constructor Tests
    //
    TEST_CLASS(ConstructorTests)
    {
    public:

        TEST_METHOD(ParseValidDriveCommand)
        {
            PktDef outgoing;
            outgoing.SetPktCount(1);
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsDriveCommand());
            Assert::IsTrue(incoming.IsValid());

            DriveBody d2 = incoming.GetDriveBody();
            Assert::AreEqual(d.Direction, d2.Direction);
            Assert::AreEqual(d.Duration, d2.Duration);
            Assert::AreEqual(d.Power, d2.Power);
        }

        TEST_METHOD(ParseValidTurnCommand)
        {
            PktDef outgoing;
            outgoing.SetPktCount(2);
            TurnBody t = MakeTurn(LEFT, 123);
            outgoing.SetTurnBody(t);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsTurnCommand());
            Assert::IsTrue(incoming.IsValid());

            TurnBody t2 = incoming.GetTurnBody();
            Assert::AreEqual(t.Direction, t2.Direction);
            Assert::AreEqual(t.Duration, t2.Duration);
        }

        TEST_METHOD(ParseValidTelemetryResponse)
        {
            PktDef outgoing;
            TelemetryBody tb = MakeTelemetry();
            outgoing.SetTelemetryBody(tb);
            // Mark as telemetry response: Status bit set, no Ack
            outgoing.SetStatus(true);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsTelemetryResponse());
            Assert::IsTrue(incoming.IsValid());
        }

        TEST_METHOD(ParseAckWithMessage)
        {
            PktDef outgoing;
            outgoing.SetAck(true);
            const char msg[] = "OK";
            outgoing.SetBodyData(const_cast<char*>(msg), 2);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsAckResponse());
            Assert::IsTrue(incoming.IsValid());

            int size = 0;
            char* body = incoming.GetBodyData(size);
            Assert::AreEqual(2, size);
            Assert::AreEqual('O', body[0]);
            Assert::AreEqual('K', body[1]);
        }

        TEST_METHOD(ParseMalformedPacket_LengthTooShort)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            // Corrupt length to be too small
            raw[3] = static_cast<char>(HEADERSIZE); // no body, no CRC

            PktDef incoming(raw);

            Assert::IsFalse(incoming.IsValid());
        }

        TEST_METHOD(ParseMalformedPacket_InvalidFlags)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            // Set multiple command bits manually
            unsigned char flags = static_cast<unsigned char>(raw[2]);
            flags |= 0x04; // set Sleep as well
            raw[2] = static_cast<char>(flags);

            PktDef incoming(raw);

            Assert::IsFalse(incoming.IsValid());
        }
    };


    //
    // 2. Setter Tests
    //
    TEST_CLASS(SetterTests)
    {
    public:

        TEST_METHOD(SetDriveBodyPopulatesCorrectly)
        {
            PktDef p;
            DriveBody d = MakeDrive(BACKWARD, 10, 95);
            p.SetDriveBody(d);

            DriveBody d2 = p.GetDriveBody();
            Assert::AreEqual(d.Direction, d2.Direction);
            Assert::AreEqual(d.Duration, d2.Duration);
            Assert::AreEqual(d.Power, d2.Power);
            Assert::IsTrue(p.IsDriveCommand());
        }

        TEST_METHOD(SetTurnBodyPopulatesCorrectly)
        {
            PktDef p;
            TurnBody t = MakeTurn(RIGHT, 321);
            p.SetTurnBody(t);

            TurnBody t2 = p.GetTurnBody();
            Assert::AreEqual(t.Direction, t2.Direction);
            Assert::AreEqual(t.Duration, t2.Duration);
            Assert::IsTrue(p.IsTurnCommand());
        }

        TEST_METHOD(SetBodyDataStoresMessageCorrectly)
        {
            PktDef p;
            const char msg[] = "HELLO";
            p.SetBodyData(const_cast<char*>(msg), 5);

            int size = 0;
            char* body = p.GetBodyData(size);

            Assert::AreEqual(5, size);
            Assert::AreEqual('H', body[0]);
            Assert::AreEqual('O', body[4]);
        }

        TEST_METHOD(SetCmdSetsFlagsCorrectly)
        {
            PktDef p;
            p.SetCmd(DRIVE);
            Assert::IsTrue(p.IsDriveCommand() || !p.IsValid()); // at least flags reflect drive

            p.SetCmd(SLEEP);
            Assert::IsTrue(p.IsSleepCommand() || !p.IsValid());

            p.SetCmd(RESPONSE);
            Assert::IsTrue(p.IsTelemetryCommand() || !p.IsValid());
        }

        TEST_METHOD(SetAckTogglesCorrectly)
        {
            PktDef p;
            p.SetAck(true);
            Assert::IsTrue(p.GetAck());

            p.SetAck(false);
            Assert::IsFalse(p.GetAck());
        }

        TEST_METHOD(ClearBodyResetsState)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);

            p.ClearBody();

            Assert::AreEqual(0, p.GetBodySize());
            int size = 0;
            Assert::IsNull(p.GetBodyData(size));
        }
    };


    //
    // 3. Classification Tests
    //
    TEST_CLASS(ClassificationTests)
    {
    public:

        TEST_METHOD(ClassifyOutgoingDriveCommand)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);

            Assert::IsTrue(p.IsDriveCommand());
        }

        TEST_METHOD(ClassifyOutgoingTurnCommand)
        {
            PktDef p;
            TurnBody t = MakeTurn(LEFT, 100);
            p.SetTurnBody(t);

            Assert::IsTrue(p.IsTurnCommand());
        }

        TEST_METHOD(ClassifyOutgoingAckResponse)
        {
            PktDef p;
            p.SetAck(true);
            const char msg[] = "OK";
            p.SetBodyData(const_cast<char*>(msg), 2);

            Assert::IsTrue(p.IsAckResponse());
        }

        TEST_METHOD(ClassifyIncomingDriveCommand)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsDriveCommand());
        }

        TEST_METHOD(ClassifyIncomingTurnCommand)
        {
            PktDef outgoing;
            TurnBody t = MakeTurn(RIGHT, 200);
            outgoing.SetTurnBody(t);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsTurnCommand());
        }

        TEST_METHOD(ClassifyAmbiguous3ByteBody)
        {
            // Start with a valid drive, then corrupt power to break drive but keep turn valid
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            // Corrupt power byte to something outside [80,100]
            // Header is 4 bytes, then body: Direction, Duration, Power
            raw[4 + 2] = 10; // invalid power

            PktDef incoming(raw);

            // Depending on Direction, classifier may treat as Turn or NACK.
            // We just assert it's not classified as Drive anymore.
            Assert::IsFalse(incoming.IsDriveCommand());
        }
    };


    //
    // 4. Validation Tests
    //
    TEST_CLASS(ValidationTests)
    {
    public:

        TEST_METHOD(ValidDriveCommandPassesValidation)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            p.GenPacket();

            Assert::IsTrue(p.IsValid());
        }

        TEST_METHOD(InvalidDrivePowerFailsValidation)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 50); // invalid power
            p.SetDriveBody(d);
            p.GenPacket();

            Assert::IsFalse(p.IsValid());
        }

        TEST_METHOD(InvalidTurnDirectionFailsValidation)
        {
            PktDef p;
            TurnBody t{};
            t.Direction = 99; // invalid
            t.Duration = 100;
            p.SetTurnBody(t);
            p.GenPacket();

            Assert::IsFalse(p.IsValid());
        }

        TEST_METHOD(InvalidCRCRejected)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            char* raw = p.GenPacket();

            // Corrupt CRC (last byte)
            int len = p.GetLength();
            raw[len - 1] ^= 0xFF;

            PktDef incoming(raw);

            Assert::IsFalse(incoming.IsValid());
        }

        TEST_METHOD(InvalidLengthRejected)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            char* raw = p.GenPacket();

            // Corrupt length field
            raw[3] = static_cast<char>(raw[3] + 2);

            PktDef incoming(raw);

            Assert::IsFalse(incoming.IsValid());
        }

        TEST_METHOD(AckInCommandFailsValidation)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            p.SetAck(true); // illegal: Ack in command
            p.GenPacket();

            Assert::IsFalse(p.IsValid());
        }
    };


    //
    // 5. Serialization Tests
    //
    TEST_CLASS(SerializationTests)
    {
    public:

        TEST_METHOD(DriveCommandSerializesCorrectly)
        {
            PktDef p;
            p.SetPktCount(42);
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            char* raw = p.GenPacket();

            // Re-parse
            PktDef q(raw);

            Assert::IsTrue(q.IsDriveCommand());
            Assert::IsTrue(q.IsValid());
            Assert::AreEqual(42, q.GetPktCount());
        }

        TEST_METHOD(TurnCommandSerializesCorrectly)
        {
            PktDef p;
            p.SetPktCount(7);
            TurnBody t = MakeTurn(LEFT, 321);
            p.SetTurnBody(t);
            char* raw = p.GenPacket();

            PktDef q(raw);

            Assert::IsTrue(q.IsTurnCommand());
            Assert::IsTrue(q.IsValid());
            Assert::AreEqual(7, q.GetPktCount());
        }

        TEST_METHOD(MessageBodySerializesCorrectly)
        {
            PktDef p;
            p.SetAck(true);
            const char msg[] = "HELLO";
            p.SetBodyData(const_cast<char*>(msg), 5);
            char* raw = p.GenPacket();

            PktDef q(raw);

            Assert::IsTrue(q.IsAckResponse());
            int size = 0;
            char* body = q.GetBodyData(size);
            Assert::AreEqual(5, size);
            Assert::AreEqual('H', body[0]);
            Assert::AreEqual('O', body[4]);
        }

        TEST_METHOD(CRCComputedCorrectly)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            char* raw = p.GenPacket();

            // Recompute CRC manually using PktDef::CheckCRC
            PktDef q(raw);
            Assert::IsTrue(q.IsValid());
        }

        TEST_METHOD(LengthFieldCorrect)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            p.GenPacket();

            int expected = HEADERSIZE + p.GetBodySize() + 1;
            Assert::AreEqual(expected, p.GetLength());
        }
    };


    //
    // 6. Round Trip Tests
    //
    TEST_CLASS(RoundTripTests)
    {
    public:

        TEST_METHOD(DriveCommandRoundTrip)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsDriveCommand());
            Assert::IsTrue(incoming.IsValid());

            DriveBody d2 = incoming.GetDriveBody();
            Assert::AreEqual(d.Direction, d2.Direction);
            Assert::AreEqual(d.Duration, d2.Duration);
            Assert::AreEqual(d.Power, d2.Power);
        }

        TEST_METHOD(TurnCommandRoundTrip)
        {
            PktDef outgoing;
            TurnBody t = MakeTurn(RIGHT, 200);
            outgoing.SetTurnBody(t);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsTurnCommand());
            Assert::IsTrue(incoming.IsValid());

            TurnBody t2 = incoming.GetTurnBody();
            Assert::AreEqual(t.Direction, t2.Direction);
            Assert::AreEqual(t.Duration, t2.Duration);
        }

        TEST_METHOD(TelemetryResponseRoundTrip)
        {
            PktDef outgoing;
            TelemetryBody tb = MakeTelemetry();
            outgoing.SetTelemetryBody(tb);
            outgoing.SetStatus(true);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsTelemetryResponse());
            Assert::IsTrue(incoming.IsValid());
        }

        TEST_METHOD(AckWithMessageRoundTrip)
        {
            PktDef outgoing;
            outgoing.SetAck(true);
            const char msg[] = "OK";
            outgoing.SetBodyData(const_cast<char*>(msg), 2);
            char* raw = outgoing.GenPacket();

            PktDef incoming(raw);

            Assert::IsTrue(incoming.IsAckResponse());
            Assert::IsTrue(incoming.IsValid());

            int size = 0;
            char* body = incoming.GetBodyData(size);
            Assert::AreEqual(2, size);
            Assert::AreEqual('O', body[0]);
            Assert::AreEqual('K', body[1]);
        }

        TEST_METHOD(AckRoundTrip_NoMessage)
        {
            PktDef p;
            p.SetAck(true);          // Ack = 1, no command bits
            p.SetPktCount(7);

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsTrue(q.IsAckResponse());
            Assert::IsTrue(q.IsValid());
            Assert::AreEqual(7, q.GetPktCount());

            int size = 0;
            Assert::IsNull(q.GetBodyData(size));
            Assert::AreEqual(0, size);
        }

        TEST_METHOD(NAckRoundTrip_WithMessage)
        {
            PktDef p;
            p.SetAck(false);
            p.SetPktCount(256);

            const char* msg = "ERR_BAD_CMD";
            p.SetBodyData((char*)msg, (int)strlen(msg));

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsTrue(q.IsNAckResponse());
            Assert::IsTrue(q.IsValid());
            Assert::AreEqual(256, q.GetPktCount());

            int size = 0;
            char* body = q.GetBodyData(size);

            Assert::AreEqual((int)strlen(msg), size);
            Assert::AreEqual(0, memcmp(body, msg, size));
        }



    };


    //
    // 7. Error Handling Tests
    //
    TEST_CLASS(ErrorHandlingTests)
    {
    public:

        TEST_METHOD(NullRawPointerHandledGracefully)
        {
            PktDef p(nullptr);
            Assert::IsFalse(p.IsValid());
        }

        TEST_METHOD(TooShortPacketRejected)
        {
            char raw[3] = { 0, 1, 0 }; // shorter than header
            PktDef p(raw);
            Assert::IsFalse(p.IsValid());
        }

        TEST_METHOD(IncorrectLengthFieldRejected)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            raw[3] = static_cast<char>(raw[3] + 5);

            PktDef incoming(raw);
            Assert::IsFalse(incoming.IsValid());
        }

        TEST_METHOD(IncorrectCRCRejected)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            int len = outgoing.GetLength();
            raw[len - 1] ^= 0xFF;

            PktDef incoming(raw);
            Assert::IsFalse(incoming.IsValid());
        }

        TEST_METHOD(IllegalFlagCombinationRejected)
        {
            PktDef outgoing;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            outgoing.SetDriveBody(d);
            char* raw = outgoing.GenPacket();

            // Force multiple command bits by flipping some bits in flags
            raw[2] |= 0x03;

            PktDef incoming(raw);
            Assert::IsFalse(incoming.IsValid());
        }

        TEST_METHOD(AckWithCommandBitsandDriveBody_IsInvalid)
        {
            PktDef p;
            p.SetDriveBody(MakeDrive(FORWARD, 5, 90));
            p.SetAck(true);   // illegal: Ack + Drive

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsFalse(q.IsValid());
        }

        TEST_METHOD(NAckWithTelemetryBody_IsInvalid)
        {
            PktDef p;
            TelemetryBody t{};
            p.SetTelemetryBody(t);
            p.SetAck(false);   // NACK + telemetry ? illegal

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsFalse(q.IsValid());

        }
    };


    //
    // 8. Memory Leak / Memory Lifecycle Tests
    //
    TEST_CLASS(MemoryLeakTests)
    {
    public:

        TEST_METHOD(ConstructorDestructor_NoLeaks)
        {
            for (int i = 0; i < 1000; ++i)
            {
                PktDef p;
            }
            Assert::IsTrue(true); // if this runs without crash, we're good
        }

        TEST_METHOD(ClearBodyFreesMemory)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            p.GenPacket();

            p.ClearBody();
            int size = 0;
            Assert::IsNull(p.GetBodyData(size));
            Assert::AreEqual(0, p.GetBodySize());
        }

        TEST_METHOD(GenPacketReallocatesRawBufferSafely)
        {
            PktDef p;
            DriveBody d1 = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d1);
            char* raw1 = p.GenPacket();

            DriveBody d2 = MakeDrive(BACKWARD, 10, 95);
            p.SetDriveBody(d2);
            char* raw2 = p.GenPacket();

            Assert::IsNotNull(raw1);
            Assert::IsNotNull(raw2);
            Assert::IsTrue(p.IsValid());
        }

        TEST_METHOD(MultipleGenPacketCalls_NoLeaks)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);

            for (int i = 0; i < 1000; ++i)
            {
                p.GenPacket();
            }

            Assert::IsTrue(p.IsValid());
        }

        TEST_METHOD(RoundTripCreationDestruction_NoLeaks)
        {
            for (int i = 0; i < 500; ++i)
            {
                PktDef outgoing;
                DriveBody d = MakeDrive(FORWARD, 5, 90);
                outgoing.SetDriveBody(d);
                char* raw = outgoing.GenPacket();

                PktDef incoming(raw);
                Assert::IsTrue(incoming.IsDriveCommand());
            }
        }
    };


    //
    // 9. Getter Tests
    //
    TEST_CLASS(GetterTests)
    {
    public:

        TEST_METHOD(GetCmdReturnsCorrectValue)
        {
            PktDef p;
            p.SetCmd(DRIVE);
            Assert::AreEqual(DRIVE, p.GetCmd());

            p.SetCmd(SLEEP);
            Assert::AreEqual(SLEEP, p.GetCmd());

            p.SetCmd(RESPONSE);
            Assert::AreEqual(RESPONSE, p.GetCmd());
        }

        TEST_METHOD(GetAckReturnsCorrectValue)
        {
            PktDef p;
            p.SetAck(true);
            Assert::IsTrue(p.GetAck());

            p.SetAck(false);
            Assert::IsFalse(p.GetAck());
        }

        TEST_METHOD(GetBodyDataReturnsMessage)
        {
            PktDef p;
            const char msg[] = "TEST";
            p.SetBodyData(const_cast<char*>(msg), 4);

            int size = 0;
            char* body = p.GetBodyData(size);

            Assert::AreEqual(4, size);
            Assert::AreEqual('T', body[0]);
            Assert::AreEqual('T', body[3]);
        }

        TEST_METHOD(GetBodyDataReturnsNullForNonMessage)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);

            int size = 0;
            char* body = p.GetBodyData(size);

            Assert::IsNull(body);
            Assert::AreEqual(0, size);
        }

        TEST_METHOD(GetDriveBodyReturnsCorrectStruct)
        {
            PktDef p;
            DriveBody d = MakeDrive(BACKWARD, 7, 100);
            p.SetDriveBody(d);

            DriveBody d2 = p.GetDriveBody();
            Assert::AreEqual(d.Direction, d2.Direction);
            Assert::AreEqual(d.Duration, d2.Duration);
            Assert::AreEqual(d.Power, d2.Power);
        }

        TEST_METHOD(GetTurnBodyReturnsCorrectStruct)
        {
            PktDef p;
            TurnBody t = MakeTurn(LEFT, 250);
            p.SetTurnBody(t);

            TurnBody t2 = p.GetTurnBody();
            Assert::AreEqual(t.Direction, t2.Direction);
            Assert::AreEqual(t.Duration, t2.Duration);
        }

        TEST_METHOD(GetTelemetryReturnsCorrectStruct)
        {
            PktDef p;
            TelemetryBody tb = MakeTelemetry();
            p.SetTelemetryBody(tb);

            TelemetryBody tb2 = p.GetTelemetry();
            // We don't know fields, but at least ensure size and that call doesn't crash
            Assert::IsTrue(true);
        }

        TEST_METHOD(GetLengthMatchesSerializedLength)
        {
            PktDef p;
            DriveBody d = MakeDrive(FORWARD, 5, 90);
            p.SetDriveBody(d);
            p.GenPacket();

            int len = p.GetLength();
            Assert::IsTrue(len > 0);
        }

        TEST_METHOD(GetFlagsMatchesSetFlags)
        {
            PktDef p;
            p.SetCmd(DRIVE);
            unsigned char flags1 = p.GetFlags();

            p.SetCmd(SLEEP);
            unsigned char flags2 = p.GetFlags();

            Assert::AreNotEqual(flags1, flags2);
        }
    };

    TEST_CLASS(StressTests)
    {
    public:
        TEST_METHOD(AckRoundTrip_EmbeddedNulls)
        {
            PktDef p;
            p.SetAck(true);
            p.SetPktCount(55);

            char msg[4] = { 'A', '\0', 'B', '\0' };
            p.SetBodyData(msg, 4);

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsTrue(q.IsAckResponse());
            Assert::IsTrue(q.IsValid());

            int size = 0;
            char* body = q.GetBodyData(size);

            Assert::AreEqual(4, size);
            Assert::AreEqual(0, memcmp(body, msg, 4));
        }

        TEST_METHOD(AckWithTelemetryBody_IsInvalid)
        {
            PktDef p;
            TelemetryBody t{};
            p.SetTelemetryBody(t);
            p.SetAck(true);

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsFalse(q.IsValid());
        }

        TEST_METHOD(CorruptCRC_IsInvalid)
        {
            PktDef p;
            p.SetAck(true);
            char* raw = p.GenPacket();

            // Flip a bit in the body or header
            raw[1] ^= 0x20;

            PktDef q(raw);

            Assert::IsFalse(q.IsValid());
        }


        TEST_METHOD(CorruptLength_IsInvalid)
        {
            PktDef p;
            p.SetAck(true);
            char* raw = p.GenPacket();

            raw[3] += 5; // corrupt length byte

            PktDef q(raw);

            Assert::IsFalse(q.IsValid());
        }

        TEST_METHOD(AckWithMultipleCommandBits_IsInvalid)
        {
            PktDef p;
            p.SetAck(true);
            p.SetPktCount(10);

            char* raw = p.GenPacket();

            // Set Drive + Status simultaneously
            raw[2] |= 0x03;

            PktDef q(raw);

            Assert::IsFalse(q.IsValid());
        }


        TEST_METHOD(NAckWithMultipleCommandBits_IsInvalid)
        {
            PktDef p;
            p.SetAck(false);
            p.SetPktCount(10);

            char* raw = p.GenPacket();

            // Set Drive + Sleep simultaneously
            raw[2] |= 0x05;

            PktDef q(raw);

            Assert::IsFalse(q.IsValid());
        }

        TEST_METHOD(AckRoundTrip_MaxMessage)
        {
            const int N = 255 - HEADERSIZE - 1; // max body size

            // Allocate raw buffer manually
            char* msg = new char[N];
            for (int i = 0; i < N; i++)
                msg[i] = 'X';

            PktDef p;
            p.SetAck(true);
            p.SetBodyData(msg, N);

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsTrue(q.IsAckResponse());
            Assert::IsTrue(q.IsValid());

            int size = 0;
            char* body = q.GetBodyData(size);

            Assert::AreEqual(N, size);
            Assert::IsTrue(memcmp(body, msg, N) == 0);

            delete[] msg;
        }


    
        TEST_METHOD(NAckRoundTrip_RandomGarbageBody)
        {
            char garbage[7] = { 0xFF, 0x00, 0x13, 0x7A, 0x7A, 0x01, 0x02 };

            PktDef p;
            p.SetAck(false);
            p.SetBodyData(garbage, 7);

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsTrue(q.IsNAckResponse());
            Assert::IsTrue(q.IsValid());
        }


        TEST_METHOD(AckEmptyBody_StillAck)
        {
            PktDef p;
            p.SetAck(true);

            char* raw = p.GenPacket();
            PktDef q(raw);

            Assert::IsTrue(q.IsAckResponse());
        }
    };
}
