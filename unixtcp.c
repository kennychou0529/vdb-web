
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define vdb_listen_port 8000
#define vdb_str(X) #X
#define vdb_xstr(X) vdb_str(X)
const char *vdb_listen_port_str = vdb_xstr(vdb_listen_port);

static int listen_socket = 0;
static int client_socket = 0;
static int has_listen_socket = 0;
static int has_client_socket = 0;

int tcp_listen()
{
    struct addrinfo *a = 0;
    struct addrinfo *info = 0;
    struct addrinfo hints = {0};
    has_listen_socket = 0;

    // Fetch available addresses
    hints.ai_family = AF_UNSPEC; // Don't care if ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM; // Tcp
    hints.ai_flags = AI_PASSIVE; // Arbitrary IP?
    if (getaddrinfo(0, vdb_listen_port_str, &hints, &info) != 0)
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

        if (port != vdb_listen_port)
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
        has_listen_socket = 1;
        break;
    }
    freeaddrinfo(info);
    if (!has_listen_socket)
        return 0;
    if (listen(listen_socket, 1) == -1)
    {
        has_listen_socket = 0;
        close(listen_socket);
        return 0;
    }
    return 1;
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
    return 1;
}

void tcp_close()
{
    if (has_listen_socket)
        close(listen_socket);
    has_listen_socket = 0;
}

int tcp_block_send(void *data, int size)
{
    if (!has_client_socket)
    {
        printf("[vdb] Error. The programmer forgot to ensure that a TCP client socket is valid before calling tcp_send.\n");
        return 0;
    }
    int sent_bytes = send(client_socket, data, size, 0);
    if (sent_bytes > 0) return sent_bytes;
    return 0;
}

int tcp_block_recv(void *buffer, int capacity)
{
    if (!has_client_socket)
    {
        printf("[vdb] Error. The programmer forgot to ensure that a TCP client socket is valid before calling tcp_send.\n");
        return 0;
    }
    int read_bytes = recv(client_socket, buffer, capacity, 0);
    if (read_bytes > 0) return read_bytes;
    return 0;
}


int main()
{
    if (!tcp_listen())
    {
        printf("[vdb] Error. Are you really allowed to use port '%s'?", vdb_listen_port_str);
        return 0;
    }
    printf("Listening to port %s\n", vdb_listen_port_str);
    do
    {
        printf("Waiting for client.\n");
    } while(!tcp_accept());
    tcp_close();
}
