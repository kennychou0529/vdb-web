// tcp.c - Version 1 - Blocking TCP sockets for windows and unix

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
#else
#include "tcp_unix.c"
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
