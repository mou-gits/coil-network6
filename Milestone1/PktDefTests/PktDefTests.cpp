#include "pch.h"
#include "CppUnitTest.h"
#include "../PktDefLib/PktDef.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PktDefTests
{
    TEST_CLASS(PktDefBasicTests)
    {
    public:

        TEST_METHOD(SetPktCount_StoresValue)
        {
            PktDef pkt;

            pkt.SetPktCount(25);

            Assert::AreEqual(25, pkt.GetPktCount());
        }

        TEST_METHOD(SetCmd_Drive_SetsDriveCommand)
        {
            PktDef pkt;

            pkt.SetCmd(DRIVE);

            Assert::AreEqual((int)DRIVE, (int)pkt.GetCmd());
            Assert::IsFalse(pkt.GetAck());
        }

        TEST_METHOD(SetCmd_Sleep_SetsSleepCommand)
        {
            PktDef pkt;

            pkt.SetCmd(SLEEP);

            Assert::AreEqual((int)SLEEP, (int)pkt.GetCmd());
        }

        TEST_METHOD(SetCmd_Response_SetsStatusCommand)
        {
            PktDef pkt;

            pkt.SetCmd(RESPONSE);

            Assert::AreEqual((int)RESPONSE, (int)pkt.GetCmd());
        }

        TEST_METHOD(SetCmd_ChangingCommand_ReplacesOldCommand)
        {
            PktDef pkt;

            pkt.SetCmd(DRIVE);
            Assert::AreEqual((int)DRIVE, (int)pkt.GetCmd());

            pkt.SetCmd(SLEEP);
            Assert::AreEqual((int)SLEEP, (int)pkt.GetCmd());

            pkt.SetCmd(RESPONSE);
            Assert::AreEqual((int)RESPONSE, (int)pkt.GetCmd());
        }

        TEST_METHOD(SetBodyData_ValidBody_CopiesDataAndUpdatesLength)
        {
            PktDef pkt;

            char testData[3] = { 1, 5, 80 };

            pkt.SetBodyData(testData, 3);

            Assert::IsNotNull(pkt.GetBodyData());
            Assert::AreEqual(8, pkt.GetLength());   // 4 header + 3 body + 1 CRC
            Assert::AreEqual((char)1, pkt.GetBodyData()[0]);
            Assert::AreEqual((char)5, pkt.GetBodyData()[1]);
            Assert::AreEqual((char)80, pkt.GetBodyData()[2]);
        }

        TEST_METHOD(SetBodyData_NullBody_SetsLengthToHeaderPlusCRC)
        {
            PktDef pkt;

            pkt.SetBodyData(nullptr, 0);

            Assert::IsNull(pkt.GetBodyData());
            Assert::AreEqual(5, pkt.GetLength());   // 4 header + 1 CRC
        }

        TEST_METHOD(SetBodyData_ReplacesOldBody)
        {
            PktDef pkt;

            char firstData[3] = { 1, 5, 80 };
            char secondData[2] = { 4, 9 };

            pkt.SetBodyData(firstData, 3);
            pkt.SetBodyData(secondData, 2);

            Assert::IsNotNull(pkt.GetBodyData());
            Assert::AreEqual(7, pkt.GetLength());   // 4 + 2 + 1
            Assert::AreEqual((char)4, pkt.GetBodyData()[0]);
            Assert::AreEqual((char)9, pkt.GetBodyData()[1]);
        }
        TEST_METHOD(ParseConstructor_HandlesNoBodyPacket)
        {
            PktDef original;
            original.SetPktCount(5);
            original.SetCmd(SLEEP);
            original.SetBodyData(nullptr, 0);

            char* raw = original.GenPacket();

            PktDef parsed(raw);

            Assert::AreEqual(5, parsed.GetPktCount());
            Assert::AreEqual((int)SLEEP, (int)parsed.GetCmd());
            Assert::IsNull(parsed.GetBodyData());
        }
    };
}