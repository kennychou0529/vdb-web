// tcp.c: Blocking TCP sockets for windows and linux

// interface
int tcp_listen(int port);
int tcp_accept();
int tcp_close();
int tcp_send(const void *data, int size);
int tcp_recv(void *buffer, int capacity);

// implementation
// @ move socket handles and has_* into vdb.c??
static int client_socket = 0;
static int listen_socket = 0;
static int has_client_socket = 0;
static int has_listen_socket = 0;

#if defined(_WIN32) || defined(_WIN64)
#include "tcp_win.c"
#else
#include "tcp_unix.c"
#endif

int tcp_send(const void *data, int size)
{
    vdb_assert(has_client_socket);
    int sent_bytes = send(client_socket, data, size, 0);
    if (sent_bytes > 0) return sent_bytes;
    return 0;
}

int tcp_recv(void *buffer, int capacity)
{
    vdb_assert(has_client_socket);
    int read_bytes = recv(client_socket, buffer, capacity, 0);
    if (read_bytes > 0) return read_bytes;
    return 0;
}
