#pragma once

#include <string>

#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SocketHandle;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERR SOCKET_ERROR
    #define SHUT_FLAG SD_BOTH
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <cstring>
    typedef int SocketHandle;
    #define INVALID_SOCK (-1)
    #define SOCKET_ERR (-1)
    #define SHUT_FLAG SHUT_RDWR
    inline int closesocket(int fd) { return close(fd); }
#endif

const int DEFAULT_SIZE = 1024;

enum SocketType { CLIENT, SERVER };
enum ConnectionType { TCP, UDP };

class MySocket
{
public:
    MySocket(SocketType type, std::string ip, unsigned int port,
             ConnectionType connType, unsigned int bufSize);
    ~MySocket();

    void ConnectTCP();
    void DisconnectTCP();
    void SendData(const char* data, int size);
    int  GetData(char* destBuffer);

    std::string GetIPAddr() const;
    void        SetIPAddr(const std::string& ip);
    void SetPort(int port);
    int  GetPort() const;
    SocketType GetType() const;
    void       SetType(SocketType type);

private:
    void InitPlatform();
    void CleanupPlatform();
    void SetupServer();
    void SetupUDPServer();

    char* Buffer;
    SocketHandle WelcomeSocket;
    SocketHandle ConnectionSocket;
    sockaddr_in SvrAddr;

    SocketType mySocket;
    ConnectionType connectionType;
    std::string IPAddr;
    int Port;
    bool bTCPConnect;
    int MaxSize;

#ifdef _WIN32
    static int s_winsockRefCount;
#endif
};
