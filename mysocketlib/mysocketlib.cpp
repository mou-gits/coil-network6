#include "MySocket.h"
#include <stdexcept>
#include <cstring>

#ifdef _WIN32
    #include <ws2tcpip.h>
    int MySocket::s_winsockRefCount = 0;
#endif

// ===============================
// Platform init/cleanup
// ===============================

void MySocket::InitPlatform()
{
#ifdef _WIN32
    if (s_winsockRefCount == 0)
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            throw std::runtime_error("WSAStartup failed");
    }
    ++s_winsockRefCount;
#endif
}

void MySocket::CleanupPlatform()
{
#ifdef _WIN32
    --s_winsockRefCount;
    if (s_winsockRefCount == 0)
        WSACleanup();
#endif
}

// ===============================
// Constructor
// ===============================

MySocket::MySocket(SocketType type, std::string ip, unsigned int port,
                   ConnectionType connType, unsigned int bufSize)
    : Buffer(nullptr),
      WelcomeSocket(INVALID_SOCK),
      ConnectionSocket(INVALID_SOCK),
      mySocket(type),
      connectionType(connType),
      IPAddr(ip),
      Port(static_cast<int>(port)),
      bTCPConnect(false),
      MaxSize(bufSize > 0 ? static_cast<int>(bufSize) : DEFAULT_SIZE)
{
    InitPlatform();

    Buffer = new char[MaxSize];

    std::memset(&SvrAddr, 0, sizeof(SvrAddr));
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_port = htons(static_cast<unsigned short>(Port));
    SvrAddr.sin_addr.s_addr = inet_addr(IPAddr.c_str());

    if (mySocket == SERVER)
    {
        if (connectionType == TCP)
            SetupServer();
        else
            SetupUDPServer();
    }
    else
    {
        if (connectionType == UDP)
        {
            ConnectionSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (ConnectionSocket == INVALID_SOCK)
                throw std::runtime_error("Failed to create UDP client socket");
        }
    }
}

// ===============================
// Destructor
// ===============================

MySocket::~MySocket()
{
    if (ConnectionSocket != INVALID_SOCK)
    {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCK;
    }

    if (WelcomeSocket != INVALID_SOCK)
    {
        closesocket(WelcomeSocket);
        WelcomeSocket = INVALID_SOCK;
    }

    delete[] Buffer;
    Buffer = nullptr;

    CleanupPlatform();
}

// ===============================
// Server setup
// ===============================

void MySocket::SetupServer()
{
    WelcomeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (WelcomeSocket == INVALID_SOCK)
        throw std::runtime_error("Failed to create TCP welcome socket");

    if (bind(WelcomeSocket, reinterpret_cast<sockaddr*>(&SvrAddr), sizeof(SvrAddr)) == SOCKET_ERR)
        throw std::runtime_error("bind() failed on server");

    if (listen(WelcomeSocket, SOMAXCONN) == SOCKET_ERR)
        throw std::runtime_error("listen() failed on server");
}

void MySocket::SetupUDPServer()
{
    ConnectionSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ConnectionSocket == INVALID_SOCK)
        throw std::runtime_error("Failed to create UDP server socket");

    if (bind(ConnectionSocket, reinterpret_cast<sockaddr*>(&SvrAddr), sizeof(SvrAddr)) == SOCKET_ERR)
        throw std::runtime_error("bind() failed on UDP server");
}

// ===============================
// Connect / Disconnect (TCP)
// ===============================

void MySocket::ConnectTCP()
{
    if (connectionType != TCP || mySocket != CLIENT)
        return;
    if (bTCPConnect)
        return;

    ConnectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectionSocket == INVALID_SOCK)
        throw std::runtime_error("Failed to create TCP client socket");

    if (connect(ConnectionSocket, reinterpret_cast<sockaddr*>(&SvrAddr),
                sizeof(SvrAddr)) == SOCKET_ERR)
    {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCK;
        throw std::runtime_error("connect() failed");
    }

    bTCPConnect = true;
}

void MySocket::DisconnectTCP()
{
    if (connectionType != TCP)
        return;

    if (ConnectionSocket != INVALID_SOCK)
    {
        shutdown(ConnectionSocket, SHUT_FLAG);
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCK;
    }

    bTCPConnect = false;
}

// ===============================
// Send / Receive
// ===============================

void MySocket::SendData(const char* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    if (connectionType == TCP)
    {
        if (mySocket == SERVER)
        {
            if (ConnectionSocket == INVALID_SOCK)
            {
                sockaddr_in clientAddr;
                socklen_t addrLen = sizeof(clientAddr);
                ConnectionSocket = accept(WelcomeSocket,
                    reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
                if (ConnectionSocket == INVALID_SOCK)
                    throw std::runtime_error("accept() failed in SendData");
            }
        }
        else
        {
            if (!bTCPConnect || ConnectionSocket == INVALID_SOCK)
                throw std::runtime_error("TCP client not connected");
        }

        if (send(ConnectionSocket, data, size, 0) == SOCKET_ERR)
            throw std::runtime_error("send() failed");
    }
    else
    {
        if (sendto(ConnectionSocket, data, size, 0,
                   reinterpret_cast<sockaddr*>(&SvrAddr), sizeof(SvrAddr)) == SOCKET_ERR)
            throw std::runtime_error("sendto() failed");
    }
}

int MySocket::GetData(char* destBuffer)
{
    if (destBuffer == nullptr)
        return 0;

    int received = 0;

    if (connectionType == TCP)
    {
        if (mySocket == SERVER)
        {
            if (ConnectionSocket == INVALID_SOCK)
            {
                sockaddr_in clientAddr;
                socklen_t addrLen = sizeof(clientAddr);
                ConnectionSocket = accept(WelcomeSocket,
                    reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
                if (ConnectionSocket == INVALID_SOCK)
                    throw std::runtime_error("accept() failed in GetData");
            }
        }
        else
        {
            if (!bTCPConnect || ConnectionSocket == INVALID_SOCK)
                throw std::runtime_error("TCP client not connected");
        }

        received = recv(ConnectionSocket, Buffer, MaxSize, 0);
        if (received == SOCKET_ERR)
            throw std::runtime_error("recv() failed");
    }
    else
    {
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        received = recvfrom(ConnectionSocket, Buffer, MaxSize, 0,
                            reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
        if (received == SOCKET_ERR)
            throw std::runtime_error("recvfrom() failed");
    }

    if (received > 0)
        std::memcpy(destBuffer, Buffer, received);

    return received;
}

// ===============================
// Getters / Setters
// ===============================

std::string MySocket::GetIPAddr() const { return IPAddr; }

void MySocket::SetIPAddr(const std::string& ip)
{
    if (connectionType == TCP && bTCPConnect)
        throw std::runtime_error("Cannot change IP after TCP connection established");
    IPAddr = ip;
    SvrAddr.sin_addr.s_addr = inet_addr(IPAddr.c_str());
}

void MySocket::SetPort(int port)
{
    if (connectionType == TCP && bTCPConnect)
        throw std::runtime_error("Cannot change port after TCP connection established");
    Port = port;
    SvrAddr.sin_port = htons(static_cast<unsigned short>(Port));
}

int MySocket::GetPort() const { return Port; }

SocketType MySocket::GetType() const { return mySocket; }

void MySocket::SetType(SocketType type)
{
    if (connectionType == TCP &&
        (WelcomeSocket != INVALID_SOCK || bTCPConnect))
        throw std::runtime_error("Cannot change SocketType while TCP is active");
    mySocket = type;
}
