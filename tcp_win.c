#define WIN32_LEAN_AND_MEAN
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")

int tcp_listen(int port)
{
    struct sockaddr_in address;
    struct WSAData wsa;
    has_listen_socket = 0;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        return 0;
    }
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return 0;
    }

    // Set the SO_REUSEADDR flag so that we don't hog the port if the program
    // is shut down before closing the socket.
    DWORD yes = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) != 0)
    {
         WSACleanup();
         return 0;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);
    if (bind(listen_socket, (const struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        return 0;
    }
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        return 0;
    }
    has_listen_socket = 1;
    return 1;
}

int tcp_accept()
{
    client_socket = accept(listen_socket, 0, 0);
    if (client_socket == INVALID_SOCKET)
    {
        closesocket(listen_socket);
        WSACleanup();
        return 0;
    }
    has_client_socket = 1;
    return 1;
}

int tcp_send(const void *data, int size, int *sent_bytes)
{
    *sent_bytes = send(client_socket, (const char*)data, size, 0);
    if (*sent_bytes >= 0) return 1;
    else return 0;
}

int tcp_recv(void *buffer, int capacity, int *read_bytes)
{
    *read_bytes = recv(client_socket, (char*)buffer, capacity, 0);
    if (*read_bytes > 0) return 1;
    else return 0;
}

int tcp_shutdown()
{
    if (has_client_socket) closesocket(client_socket);
    if (has_listen_socket) closesocket(listen_socket);
    has_client_socket = 0;
    has_listen_socket = 0;
    WSACleanup();
    return 1;
}

int tcp_close_client()
{
    has_client_socket = 0;
    closesocket(client_socket);
    return 1;
}
