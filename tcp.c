// tcp.c
// Blocking TCP sockets for windows and unix

// interface

int tcp_listen(int port);
int tcp_accept();
int tcp_close();
int tcp_send(const void *data, int size);
int tcp_recv(void *buffer, int capacity);

// implementation

static int client_socket = 0;
static int listen_socket = 0;
static int has_client_socket = 0;
static int has_listen_socket = 0;

#if defined(_WIN32) || defined(_WIN64)
#include "tcp_win.c"
#else
#include "tcp_unix.c"
#endif

int tcp_send(void *data, int size)
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

int tcp_recv(void *buffer, int capacity)
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
