#include "pch.h"
#include "CppUnitTest.h"

#include "../mysocketlib/MySocket.h"
#include <thread>
#include <chrono>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

static const char* LOCALHOST = "127.0.0.1";
static const int TEST_PORT = 50000;

// Small helper to avoid race conditions
static void SleepShort()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
namespace MySocketTests
{
    // ================================================================
    //  TEST CLASS 1 — Constructor & Initialization
    // ================================================================
    TEST_CLASS(ConstructorTests)
    {
    public:

        TEST_METHOD(Constructor_AllocatesBuffer)
        {
            MySocket s(CLIENT, LOCALHOST, TEST_PORT, TCP, 256);
            Assert::IsNotNull(s.GetIPAddr().c_str());
            Assert::AreEqual(TEST_PORT, s.GetPort());
        }

        TEST_METHOD(Constructor_TCPServerCreatesWelcomeSocket)
        {
            MySocket server(SERVER, LOCALHOST, TEST_PORT, TCP, 256);
            // If no exception thrown, server is valid
            Assert::AreEqual(TEST_PORT, server.GetPort());
        }

        TEST_METHOD(Constructor_UDPServerBinds)
        {
            MySocket server(SERVER, LOCALHOST, TEST_PORT, UDP, 256);
            Assert::AreEqual(TEST_PORT, server.GetPort());
        }
    };

    // ================================================================
    //  TEST CLASS 2 — TCP Connection Tests
    // ================================================================
    TEST_CLASS(TCPConnectionTests)
    {
    public:

        TEST_METHOD(TCP_ClientConnectsToServer)
        {
            MySocket server(SERVER, LOCALHOST, TEST_PORT, TCP, 256);

            // Start client
            MySocket client(CLIENT, LOCALHOST, TEST_PORT, TCP, 256);
            client.ConnectTCP();

            SleepShort();

            // Send 1 byte to trigger accept() and recv()
            const char* msg = "X";
            client.SendData(msg, 1);

            char buffer[256] = {};
            int received = server.GetData(buffer);

            Assert::AreEqual(1, received);
            Assert::AreEqual('X', buffer[0]);
        }

    };

    // ================================================================
    //  TEST CLASS 3 — TCP Send/Receive Tests
    // ================================================================
    TEST_CLASS(TCPSendReceiveTests)
    {
    public:

        TEST_METHOD(TCP_SendReceive_Works)
        {
            MySocket server(SERVER, LOCALHOST, TEST_PORT, TCP, 256);
            MySocket client(CLIENT, LOCALHOST, TEST_PORT, TCP, 256);

            client.ConnectTCP();
            SleepShort();

            const char* msg = "Hello";
            client.SendData(msg, 5);

            char buffer[256] = {};
            int received = server.GetData(buffer);

            Assert::AreEqual(5, received);
            Assert::AreEqual(std::string("Hello"), std::string(buffer, received));
        }
    };

    // ================================================================
    //  TEST CLASS 4 — UDP Send/Receive Tests
    // ================================================================
    TEST_CLASS(UDPSendReceiveTests)
    {
    public:

        TEST_METHOD(UDP_SendReceive_Works)
        {
            const int SERVER_PORT = 50001;   // different port
            const int CLIENT_PORT = 0;       // client uses ephemeral port

            MySocket server(SERVER, LOCALHOST, SERVER_PORT, UDP, 256);
            MySocket client(CLIENT, LOCALHOST, SERVER_PORT, UDP, 256);

            const char* msg = "Ping";
            client.SendData(msg, 4);

            char buffer[256] = {};
            int received = server.GetData(buffer);

            Assert::AreEqual(4, received);
            Assert::AreEqual(std::string("Ping"), std::string(buffer, received));
        }

    };

    // ================================================================
    //  TEST CLASS 5 — Getter/Setter Tests
    // ================================================================
    TEST_CLASS(GetterSetterTests)
    {
    public:

        TEST_METHOD(SetIPAddr_BeforeConnection_Works)
        {
            MySocket c(CLIENT, LOCALHOST, TEST_PORT, TCP, 256);
            c.SetIPAddr("192.168.1.10");
            Assert::AreEqual(std::string("192.168.1.10"), c.GetIPAddr());
        }

        TEST_METHOD(SetPort_BeforeConnection_Works)
        {
            MySocket c(CLIENT, LOCALHOST, TEST_PORT, TCP, 256);
            c.SetPort(60000);
            Assert::AreEqual(60000, c.GetPort());
        }

        TEST_METHOD(SetIPAddr_AfterConnection_Throws)
        {
            MySocket server(SERVER, LOCALHOST, TEST_PORT, TCP, 256);
            MySocket client(CLIENT, LOCALHOST, TEST_PORT, TCP, 256);

            client.ConnectTCP();
            SleepShort();

            Assert::ExpectException<std::runtime_error>([&]() {
                client.SetIPAddr("10.0.0.5");
                });
        }

        TEST_METHOD(SetType_AfterServerListening_Throws)
        {
            MySocket server(SERVER, LOCALHOST, TEST_PORT, TCP, 256);

            Assert::ExpectException<std::runtime_error>([&]() {
                server.SetType(CLIENT);
                });
        }
    };
}
