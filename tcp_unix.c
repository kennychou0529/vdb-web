#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int tcp_listen(int listen_port)
{
    struct addrinfo *a = 0;
    struct addrinfo *info = 0;
    struct addrinfo hints = {0};
    has_listen_socket = 0;

    char listen_port_str[64];
    sprintf(listen_port_str, "%d", listen_port);

    // Fetch available addresses
    hints.ai_family = AF_UNSPEC; // Don't care if ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM; // Tcp
    hints.ai_flags = AI_PASSIVE; // Arbitrary IP?
    if (getaddrinfo(0, listen_port_str, &hints, &info) != 0)
        return 0;

    // Bind socket to first available
    for (a = info; a != 0; a = a->ai_next)
    {
        int yes = 1;
        int port = 0;

        if (a->ai_addr->sa_family == AF_INET)
            port = ntohs((int)((struct sockaddr_in*)a->ai_addr)->sin_port);
        else
            port = ntohs((int)((struct sockaddr_in6*)a->ai_addr)->sin6_port);

        if (port != listen_port)
            continue;

        listen_socket = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (listen_socket == -1)
            continue;

        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            continue;

        if (bind(listen_socket, a->ai_addr, a->ai_addrlen) == -1)
        {
            close(listen_socket);
            continue;
        }
        if (listen(listen_socket, 1) == -1)
        {
            close(listen_socket);
            return 0;
        }
        has_listen_socket = 1;
        break;
    }
    freeaddrinfo(info);
    return has_listen_socket;
}

int tcp_accept()
{
    has_client_socket = 0;
    if (!has_listen_socket)
    {
        printf("[vdb] Error. The programmer forgot to ensure that a TCP listen socket exists before calling tcp_accept.\n");
        return 0;
    }
    client_socket = accept(listen_socket, 0, 0);
    if (client_socket == -1)
        return 0;
    has_client_socket = 1;
    return 1;
}

int tcp_close()
{
    if (has_listen_socket)
        close(listen_socket);
    if (has_client_socket)
        close(client_socket);
    has_client_socket = 0;
    has_listen_socket = 0;
    return 1;
}

