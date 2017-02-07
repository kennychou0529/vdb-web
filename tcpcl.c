#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "tcp.c"

int recv_thread()
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
    while (1)
    {
        printf("> ");
        scanf("%s", data);
        if (tcp_send(data, strlen(data)+1) == 0)
            break;
    }
    printf("Oh shit\n");
}

#if defined(_WIN32) || defined(_WIN64)
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

DWORD WINAPI win_recv_thread(void *vdata) { return recv_thread(); }
void create_recv_thread() { CreateThread(0, 0, win_recv_thread, NULL, 0, 0); }
#else

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

int create_recv_thread()
{
    pid_t pid = fork();
    if (pid == -1)
    {
        return 0;
    }
    if (pid == 0) // is child
    {
        recv_thread();
        _exit(0); // http://unix.stackexchange.com/questions/91058/file-descriptor-and-fork
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc == 3) // client
    {
        char *addr = argv[1];
        char *port = argv[2];
        printf("Connecting to %s:%s\n", addr, port);
        while (!tcp_connect(addr, port))
        {
            // block
        }
        create_recv_thread();
        send_thread();
        tcp_close();
    }
    else if (argc == 2) // server
    {
        int port = atoi(argv[1]);
        if (!tcp_listen(port))
        {
            printf("Are you really allowed to use port %d?", port);
            return 0;
        }
        do
        {
            printf("Waiting for client on port %d.\n", port);
        } while(!tcp_accept());
        create_recv_thread();
        send_thread();
        tcp_close();
    }
    else
    {
        printf("usage:\n");
        printf("tcpcl 8000: Start a server listening to port 8000\n");
        printf("tcpcl 127 0 0 1 8000: Connect a client to 127.0.0.1:8000\n");
    }
    return 0;
}
#endif
