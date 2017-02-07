#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")

static int client_socket = 0;
static int listen_socket = 0;

int tcp_accept(uint16_t port)
{
    struct sockaddr_in address;
    struct WSAData wsa;
    listen_socket = 0;
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
        printf("Could not accept client socket.\n");
        return 0;
    }
    return 1;
}

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

int tcp_send(void *data, uint32_t size)
{
    int sent_bytes = send(client_socket, (const char*)data, size, 0);
    if (sent_bytes > 0) return sent_bytes;
    return 0;
}

int tcp_recv(void *buffer, uint32_t capacity)
{
    int read_bytes = recv(client_socket, (char*)buffer, capacity, 0);
    if (read_bytes > 0) return read_bytes;
    return 0;
}

DWORD WINAPI recv_thread(void *vdata)
{
    static char data[1024*1024] = {0};
    int count = 0;
    do
    {
        count = tcp_recv(data, sizeof(data));
        if (count > 0)
            printf("sv (%d): '%s'\n", count, data);
    } while (count > 0);
    return 0;
}

void send_thread()
{
    static char data[1024*1024] = {0};
    printf("Type a message and press enter.\n");
    do
    {
        scanf("%s", data);
    } while (tcp_send(data, strlen(data)+1) != 0);
}

int main(int argc, char **argv)
{
    if (argc == 6) // client
    {
        uint8_t ip1 = (uint8_t)atoi(argv[1]);
        uint8_t ip2 = (uint8_t)atoi(argv[2]);
        uint8_t ip3 = (uint8_t)atoi(argv[3]);
        uint8_t ip4 = (uint8_t)atoi(argv[4]);
        uint16_t port = (uint16_t)atoi(argv[5]);
        printf("connecting to %d.%d.%d.%d:%d...\n", ip1, ip2, ip3, ip4, port);
        while (!tcp_connect(ip1, ip2, ip3, ip4, port))
        {
            printf("failed.\n");
        }
        printf("success!\n");
        CreateThread(0, 0, recv_thread, NULL, 0, 0);
        send_thread();
    }
    else if (argc == 2) // server
    {
        uint16_t port = (uint16_t)atoi(argv[1]);
        printf("listening to %d\n", port);
        if (!tcp_accept(port))
        {
            return 0;
        }
        printf("got a client!\n");
        CreateThread(0, 0, recv_thread, NULL, 0, 0);
        send_thread();
    }
    else
    {
        printf("usage:\n");
        printf("tcpcl 8000: Start a server listening to port 8000\n");
        printf("tcpcl 127 0 0 1 8000: Connect a client to 127.0.0.1:8000\n");
    }
    return 0;
}
