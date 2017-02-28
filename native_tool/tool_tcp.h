
#include "../tcp.c"

#if defined(_WIN32) || defined(_WIN64)
int tcp_connect(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port)
{
    struct sockaddr_in address;
    struct WSAData wsa;
    tcp_has_client_socket = 0;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        return 0;
    }
    tcp_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_client_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return 0;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl((ip0 << 24) | (ip1 << 16) | (ip2 <<  8) | ip3);
    address.sin_port = htons(port);
    if (connect(tcp_client_socket, (const struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(tcp_client_socket);
        WSACleanup();
        return 0;
    }
    tcp_has_client_socket = 1;
    return 1;
}

#else
int tcp_connect(const char *addr, const char *port)
{
    struct addrinfo hints = {0};
    struct addrinfo *info = 0;
    tcp_has_client_socket = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(addr, port, &hints, &info) == -1)
        return 0;
    tcp_client_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (tcp_client_socket == -1)
        return 0;
    if (connect(tcp_client_socket, info->ai_addr, info->ai_addrlen) == -1)
    {
        close(tcp_client_socket);
        return 0;
    }
    tcp_has_client_socket = 1;
    return 1;
}
#endif
