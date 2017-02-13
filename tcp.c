// interface
int tcp_listen(int port);
int tcp_accept(int tcp_listen_socket);
int tcp_shutdown();
int tcp_send(const void *data, int size, int *sent_bytes);
int tcp_recv(void *buffer, int capacity, int *read_bytes);
int tcp_sendall(const void *data, int size);

// implementation
#if defined(_WIN32) || defined(_WIN64)
#define TCP_WINDOWS
#else
#define TCP_UNIX
#endif
#ifdef TCP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define TCP_INVALID_SOCKET INVALID_SOCKET
#define TCP_SOCKET_ERROR SOCKET_ERROR
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#define TCP_INVALID_SOCKET -1
#define TCP_SOCKET_ERROR -1
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#endif

#ifdef TCP_WINDOWS
#define tcp_cleanup() WSACleanup()
#define tcp_close(s) closesocket(s)
#define tcp_socket_t SOCKET
#else
#define tcp_cleanup()
#define tcp_close(s) close(s)
#define tcp_socket_t int
#endif

static tcp_socket_t tcp_client_socket = 0;
static tcp_socket_t tcp_listen_socket = 0;
static int tcp_has_client_socket = 0;
static int tcp_has_listen_socket = 0;

int tcp_listen(int listen_port)
{
    #if 1
    #ifdef TCP_WINDOWS
    DWORD yes = 1;
    #else
    int yes = 1;
    #endif
    struct sockaddr_in addr = {0};
    tcp_has_listen_socket = 0;

    #ifdef TCP_WINDOWS
    struct WSAData wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
        return 0;
    #endif

    tcp_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listen_socket == TCP_INVALID_SOCKET)
    {
        printf("fuck\n");
        tcp_cleanup();
        return 0;
    }

    if (setsockopt(tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) == TCP_SOCKET_ERROR)
    {
        tcp_close(tcp_listen_socket);
        tcp_cleanup();
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)listen_port);
    if (bind(tcp_listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == TCP_SOCKET_ERROR)
    {
        tcp_close(tcp_listen_socket);
        tcp_cleanup();
        return 0;
    }

    if (listen(tcp_listen_socket, 1) == TCP_SOCKET_ERROR)
    {
        tcp_close(tcp_listen_socket);
        tcp_cleanup();
        return 0;
    }
    tcp_has_listen_socket = 1;
    return tcp_has_listen_socket;
    #else
    struct addrinfo *a = 0;
    struct addrinfo *info = 0;
    struct addrinfo hints = {0};
    char listen_port_str[64];
    sprintf(listen_port_str, "%d", listen_port);

    tcp_has_listen_socket = 0;

    #ifdef TCP_WINDOWS
    struct WSAData wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
        return 0;
    #endif

    // Fetch available addresses
    hints.ai_family = AF_UNSPEC; // Don't care if ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM; // Tcp
    hints.ai_flags = AI_PASSIVE; // Arbitrary IP?
    if (getaddrinfo(0, listen_port_str, &hints, &info) != 0)
        return 0;

    // Bind socket to first available
    for (a = info; a != 0; a = a->ai_next)
    {
        #ifdef TCP_WINDOWS
        DWORD yes = 1;
        #else
        int yes = 1;
        #endif
        int port = 0;

        if (a->ai_addr->sa_family == AF_INET)
            port = ntohs((int)((struct sockaddr_in*)a->ai_addr)->sin_port);
        else
            port = ntohs((int)((struct sockaddr_in6*)a->ai_addr)->sin6_port);
        if (port != listen_port)
            continue;

        tcp_listen_socket = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (tcp_listen_socket == TCP_INVALID_SOCKET)
            continue;

        if (setsockopt(tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) == TCP_SOCKET_ERROR)
        {
            tcp_close(tcp_listen_socket);
            tcp_cleanup();
            continue;
        }

        if (bind(tcp_listen_socket, a->ai_addr, (int)a->ai_addrlen) == TCP_SOCKET_ERROR)
        {
            tcp_close(tcp_listen_socket);
            tcp_cleanup();
            continue;
        }
        if (listen(tcp_listen_socket, 1) == TCP_SOCKET_ERROR)
        {
            tcp_close(tcp_listen_socket);
            tcp_cleanup();
            continue;
        }
        tcp_has_listen_socket = 1;
        break;
    }
    freeaddrinfo(info);
    return tcp_has_listen_socket;
    #endif
}

int tcp_accept()
{
    tcp_client_socket = accept(tcp_listen_socket, 0, 0);
    if (tcp_client_socket == TCP_INVALID_SOCKET)
    {
        tcp_close(tcp_listen_socket);
        tcp_cleanup();
        return 0;
    }
    tcp_has_client_socket = 1;
    return 1;
}

int tcp_shutdown()
{
    if (tcp_has_client_socket) tcp_close(tcp_client_socket);
    if (tcp_has_listen_socket) tcp_close(tcp_listen_socket);
    tcp_has_client_socket = 0;
    tcp_has_listen_socket = 0;
    tcp_cleanup();
    return 1;
}

int tcp_close_client()
{
    tcp_has_client_socket = 0;
    tcp_close(tcp_client_socket);
    return 1;
}

int tcp_send(const void *data, int size, int *sent_bytes)
{
    *sent_bytes = send(tcp_client_socket, (const char*)data, size, 0);
    if (*sent_bytes >= 0) return 1;
    else return 0;
}

int tcp_recv(void *buffer, int capacity, int *read_bytes)
{
    *read_bytes = recv(tcp_client_socket, (char*)buffer, capacity, 0);
    if (*read_bytes > 0) return 1;
    else return 0;
}

int tcp_sendall(const void *buffer, int bytes_to_send)
{
    int sent;
    int remaining = bytes_to_send;
    const char *ptr = (const char*)buffer;
    while (remaining > 0)
    {
        if (!tcp_send(ptr, remaining, &sent))
            return 0;
        remaining -= sent;
        ptr += sent;
        if (remaining < 0)
            return 0;
    }
    return 1;
}
