#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <string>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

// ===============================
// GLOBAL CONSTANTS / ENUMS
// ===============================

const int DEFAULT_SIZE = 1024;

enum SocketType
{
    CLIENT,
    SERVER
};

enum ConnectionType
{
    TCP,
    UDP
};

// ===============================
// CLASS MySocket
// ===============================

class MySocket
{
public:
    // Constructor
    MySocket(SocketType type,
        std::string ip,
        unsigned int port,
        ConnectionType connType,
        unsigned int bufSize);

    // Destructor
    ~MySocket();

    // Core operations
    void ConnectTCP();
    void DisconnectTCP();

    void SendData(const char* data, int size);
    int  GetData(char* destBuffer);

    // Getters / Setters
    std::string GetIPAddr() const;
    void        SetIPAddr(const std::string& ip);

    void SetPort(int port);
    int  GetPort() const;

    SocketType GetType() const;
    void       SetType(SocketType type);

private:
    // Internal helpers
    void InitWinsock();
    void CleanupWinsock();
    void SetupServer();
    void SetupUDPServer();

    // Members
    char* Buffer;
    SOCKET WelcomeSocket;
    SOCKET ConnectionSocket;
    sockaddr_in SvrAddr;

    SocketType mySocket;
    ConnectionType connectionType;

    std::string IPAddr;
    int Port;

    bool bTCPConnect;
    int MaxSize;

    static int s_winsockRefCount;
};
