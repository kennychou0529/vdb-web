// tcp.c: Blocking TCP sockets for windows and linux

// interface
int tcp_listen(int port);
int tcp_accept(int listen_socket);
int tcp_shutdown();
int tcp_send(const void *data, int size, int *sent_bytes);
int tcp_recv(void *buffer, int capacity, int *read_bytes);
int tcp_sendall(const void *data, int size);

// implementation
static int client_socket = 0;
static int listen_socket = 0;
static int has_client_socket = 0;
static int has_listen_socket = 0;

#if defined(_WIN32) || defined(_WIN64)
#include "tcp_win.c"

int tcp_connect(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port)
{
    struct sockaddr_in address;
    struct WSAData wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        return 0;
    }
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return 0;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl((ip0 << 24) | (ip1 << 16) | (ip2 <<  8) | ip3);
    address.sin_port = htons(port);
    if (connect(client_socket, (const struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(client_socket);
        WSACleanup();
        return 0;
    }
    return 1;
}

#else
#include "tcp_unix.c"

int tcp_connect(const char *addr, const char *port)
{
    struct addrinfo hints = {0};
    struct addrinfo *info = 0;
    has_client_socket = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(addr, port, &hints, &info) == -1)
        return 0;
    client_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (client_socket == -1)
        return 0;
    if (connect(client_socket, info->ai_addr, info->ai_addrlen) == -1)
    {
        close(client_socket);
        return 0;
    }
    has_client_socket = 1;
    return 1;
}
#endif

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
