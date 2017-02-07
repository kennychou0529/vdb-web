#include <stdio.h>
#include <stdint.h>

int tcp_accept(uint16_t port);
int tcp_close();
int tcp_nonblock(int enabled);
int tcp_send(const void *data, int size);
int tcp_recv(void *buffer, int capacity, int *read_bytes);

#ifdef _WIN32
#include <assert.h>
#define WIN32_LEAN_AND_MEAN
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")

static int client_socket = 0;
static int listen_socket = 0;
static int tcp_init = 0;

int tcp_accept(uint16_t port)
{
    struct sockaddr_in address;
    struct WSAData wsa;
    listen_socket = 0;
    client_socket = 0;
    tcp_init = 0;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        printf("Failed to initialize TCP socket.\n");
        return 0;
    }
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        WSACleanup();
        printf("Failed to create a TCP socket.\n");
        return 0;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(listen_socket, (const struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        printf("Failed to bind socket to port %d. Try a different port?\n", port);
        return 0;
    }
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        printf("Failed to begin listening for clients.\n");
        return 0;
    }
    client_socket = accept(listen_socket, 0, 0);
    if (client_socket == INVALID_SOCKET)
    {
        closesocket(listen_socket);
        WSACleanup();
        printf("Could not accept client socket.\n");
        return 0;
    }
    tcp_init = 1;
    return 1;
}

int tcp_nonblock(int enabled)
{
    DWORD word;
    if (!tcp_init)
    {
        printf("TCP has not been initialized.\n");
        return 0;
    }
    word = (enabled) ? 1 : 0;
    if (ioctlsocket(client_socket, FIONBIO, &word) != 0)
    {
        printf("Failed to set TCP to nonblocking.\n");
        return 0;
    }
    return 1;
}

int tcp_send(const void *data, int size)
{
    int sent_bytes;
    if (!tcp_init) return 0;
    sent_bytes = send(client_socket, (const char*)data, size, 0);
    if (sent_bytes == size)
    {
        return 1;
    }
    else
    {
        // @ handle case when send buffers are full (WSAWOULDBLOCK): http://stackoverflow.com/questions/14546362/how-to-resolve-wsaewouldblock-error
        // @ for now just say that we failed (this will trigger a shutdown)
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) return 0;
        else return 0;
    }
}

int tcp_recv(void *buffer, int capacity, int *read_bytes)
{
    if (!tcp_init) return 0;
    *read_bytes = recv(client_socket, (char*)buffer, capacity, 0);
    if (*read_bytes > 0)
    {
        return 1;
    }
    else
    {
        // @ handle WOULDBLOCK on tcp_recv if there were more data
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) return 1;
        else return 0;
    }
}

int tcp_close()
{
    if (!tcp_init)
    {
        return 1;
    }
    tcp_init = 0;
    if (client_socket)
    {
        closesocket(client_socket);
    }
    if (listen_socket)
    {
        closesocket(listen_socket);
    }
    WSACleanup();
    return 1;
}
#else
#error "Not implemented yet"
#endif
