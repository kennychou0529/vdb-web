#include <stdint.h>
#define tcp_connection_closed -1
#define tcp_would_block        0

// Opens a TCP connection to ip0.ip1.ip2.ip3:port, for example 127.0.0.1:27015.
// Returns true if successful.
bool tcp_cl_connect(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port);

// Binds a TCP socket and blocks for client connections to 'port'. If true is
// returned, subsequent calls to tcp_recv and tcp_send will address this client.
bool tcp_sv_accept(uint16_t port);

// If server, closes both client and listening socket.
// If client, closes client socket.
void tcp_close();

// If 'enabled' is true, tcp_recv and tcp_send will not block when called.
bool tcp_nonblock(bool enabled);

// Transmit a contiguous block of 'size' bytes pointed to by 'data'
// and return the number of bytes sent. Returns -1 if it failed.
int tcp_send(void *data, uint32_t size);

// Accepts as much data as is available not exceeding 'capacity' bytes
// and store the result in 'data'. Returns -1 if the stream is closed.
int tcp_recv(void *buffer, uint32_t capacity);

#ifdef _WIN32
#include <assert.h>
#include <winsock.h>
#include <iphlpapi.h>
#pragma comment(lib, "wsock32.lib")

static uint32_t client_socket = 0;
static uint32_t listen_socket = 0;
static bool tcp_init = false;

bool tcp_cl_connect(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port)
{
    listen_socket = 0;
    client_socket = 0;
    tcp_init = false;

    WSAData wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        return false;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return false;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl((ip0 << 24) | (ip1 << 16) | (ip2 <<  8) | ip3);
    address.sin_port = htons(port);

    #if 1
    // todo: is this blocking? can we make it non-blocking?
    if (connect(client_socket, (const sockaddr*)&address, sizeof(sockaddr_in)) == SOCKET_ERROR)
    {
        closesocket(client_socket);
        WSACleanup();
        return false;
    }
    #else
    // todo: is this blocking? can we make it non-blocking?
    if (connect(client_socket, (const sockaddr*)&address, sizeof(sockaddr_in)) == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
        {
            printf("Would block\n");
        }
        closesocket(client_socket);
        WSACleanup();
        assert(false);
        return false;
    }
    #endif

    tcp_init = true;
    return true;
}

bool tcp_sv_accept(uint16_t port)
{
    listen_socket = 0;
    client_socket = 0;
    tcp_init = false;

    WSAData wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        assert(false);
        return false;
    }

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        WSACleanup();
        assert(false);
        return false;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(listen_socket, (const sockaddr*)&address, sizeof(sockaddr_in)) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        assert(false);
        return false;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        assert(false);
        return false;
    }

    client_socket = accept(listen_socket, 0, 0);
    if (client_socket == INVALID_SOCKET)
    {
        closesocket(listen_socket);
        WSACleanup();
        assert(false);
        return false;
    }
    tcp_init = true;
    return true;
}

bool tcp_nonblock(bool enabled)
{
    assert(tcp_init);
    if (!tcp_init)
    {
        return false;
    }

    DWORD word = (enabled) ? 1 : 0;
    if (ioctlsocket(client_socket, FIONBIO, &word) != 0)
    {
        assert(false);
        return false;
    }
    return true;
}

int tcp_send(void *data, uint32_t size)
{
    assert(tcp_init);
    if (!tcp_init)
    {
        return -1;
    }

    int sent_bytes = send(client_socket, (const char*)data, size, 0);
    if (sent_bytes > 0)
    {
        return sent_bytes;
    }
    else
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
        {
            return tcp_would_block;
        }
        else
        {
            return tcp_connection_closed;
        }
    }
}

int tcp_recv(void *buffer, uint32_t capacity)
{
    assert(tcp_init);
    if (!tcp_init)
    {
        return -1;
    }

    int read_bytes = recv(client_socket, (char*)buffer, capacity, 0);
    if (read_bytes > 0)
    {
        return read_bytes;
    }
    else
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
        {
            return tcp_would_block;
        }
        else
        {
            return tcp_connection_closed;
        }
    }
}

void tcp_close()
{
    assert(tcp_init);
    if (!tcp_init)
    {
        return;
    }
    if (client_socket)
    {
        closesocket(client_socket);
    }
    if (listen_socket)
    {
        closesocket(listen_socket);
    }
    WSACleanup();
}
#else
#error "Not implemented yet"
#endif
