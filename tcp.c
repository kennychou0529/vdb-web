// tcp.c: Blocking TCP sockets for windows and linux

// interface
int tcp_listen(int port, int *out_listen_socket);
int tcp_accept(int listen_socket, int *out_client_socket);
int tcp_close(int s);
int tcp_send(int client_socket, const void *data, int size);
int tcp_recv(int client_socket, void *buffer, int capacity);

// implementation
#if defined(_WIN32) || defined(_WIN64)
#include "tcp_win.c"
#else
#include "tcp_unix.c"
#endif

int tcp_send(int client_socket, const void *data, int size)
{
    int sent_bytes = send(client_socket, data, size, 0);
    if (sent_bytes > 0) return sent_bytes;
    return 0;
}

int tcp_recv(int client_socket, void *buffer, int capacity)
{
    int read_bytes = recv(client_socket, buffer, capacity, 0);
    if (read_bytes > 0) return read_bytes;
    return 0;
}
