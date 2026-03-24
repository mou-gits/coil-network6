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
    };
}