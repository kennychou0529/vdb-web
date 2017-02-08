#define vdb_assert(EXPR) if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; }
#include "tcp.c"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/mman.h>

#include <fcntl.h> // for pipes
#include <unistd.h> // for pipes

struct shared_t
{
    char send_buffer[1024*1024];
    int bytes_to_send;
    int has_send_thread;

    // These pipes are used for flow control between the main thread and the sending thread
    // The sending thread blocks on a read on pipe_ready, until the main thread signals the
    // pipe by write on pipe_ready. The main thread checks if sending is complete by polling
    // (non-blocking) pipe_done, which is signalled by the sending thread.
    int ready[2]; // [0]: read, [1]: send
    int done[2];// [0]: read, [1]: send
};

static shared_t *shared = 0;

static int  wait_data_ready()   { int val = 0; return  read(shared->ready[0], &val, sizeof(val)) == sizeof(val); }
static int  signal_data_ready() { int one = 1; return write(shared->ready[1], &one, sizeof(one)) == sizeof(one); }
static int  poll_data_sent()    { int val = 0; return   read(shared->done[0], &val, sizeof(val)) == sizeof(val); }
static int  signal_data_sent()  { int one = 1; return  write(shared->done[1], &one, sizeof(one)) == sizeof(one); }
// static void close_pipes()       { close(shared->ready[0]); close(shared->ready[1]); close(shared->done[0]); close(shared->done[1]); }

void send_thread()
{
    char *send_ptr = 0;
    int remaining = 0;
    printf("created send thread\n");
    while (1)
    {
        if (!wait_data_ready()) return;

        // send all the data
        send_ptr = shared->send_buffer;
        remaining = shared->bytes_to_send;
        while (remaining > 0)
        {
            int sent_bytes;
            if (!tcp_send(send_ptr, remaining, &sent_bytes))
            {
                signal_data_sent();
                return;
            }
            remaining -= sent_bytes;
            send_ptr += sent_bytes;
        }

        if (!signal_data_sent()) return;
    }
}

void read_thread(int port)
{
    static char read_buffer[1024*1024] = {0};
    int read_bytes = 0;
    pid_t send_pid = 0;
    printf("created read thread\n");
    while (1)
    {
        if (!has_listen_socket)
        {
            printf("creating listen socket\n");
            if (!tcp_listen(port)) { usleep(1000*1000); continue; }
        }
        if (!has_client_socket)
        {
            printf("waiting for client\n");
            if (!tcp_accept()) { usleep(1000*1000); continue; }
        }
        if (!shared->has_send_thread) // @ RACECOND: UGLY: Spawn only one sending thread!!!
        {
            send_pid = fork();
            if (send_pid == -1) { printf("failed to fork\n"); usleep(1000*1000); continue; }
            if (send_pid == 0)
            {
                shared->has_send_thread = 1;
                send_thread();
                shared->has_send_thread = 0;
                _exit(0);
            }
            usleep(100*1000); // @ RACECOND: UGLY: Let child process set has_send_thread = 1
        }
        if (!tcp_recv(read_buffer, sizeof(read_buffer), &read_bytes))
        {
            printf("connection went down\n");
            tcp_shutdown();
            if (shared->has_send_thread)
            {
                kill(send_pid, SIGUSR1);
                shared->has_send_thread = 0;
            }
            usleep(1000*1000);
            continue;
        }
        printf("got (%d) '%s'\n", read_bytes, read_buffer);
    }
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
        pid_t pid = fork();
        if (pid == -1) { return 0; }
        if (pid == 0)
        {
            while (1)
            {
                char buffer[1024] = {0};
                int read;
                if (!tcp_recv(buffer, sizeof(buffer), &read)) break;
                printf("got: '%s'\n", buffer);
            }
            _exit(0);
        }
        else
        {
            while (1)
            {
                char buffer[1024];
                int sent;
                scanf("%s", buffer);
                if (!tcp_send(buffer, strlen(buffer), &sent)) break;
            }
        }
        tcp_shutdown();
    }
    else
    if (argc == 2) // server
    {
        // Share memory between main process and child processes
        shared = (shared_t*)mmap(NULL, sizeof(shared_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (shared == MAP_FAILED)
            return 0;

        // Create flow-control pipes
        if (pipe(shared->ready) == -1 || pipe2(shared->done, O_NONBLOCK) == -1)
            return 0;

        // Create reading thread (@ RACECOND: ONLY CREATE ONE READING THREAD)
        int port = atoi(argv[1]);
        pid_t pid = fork();
        if (pid == -1) { return 0; }
        if (pid == 0)  { read_thread(port); _exit(0); }

        int can_send = 1;
        while (1)
        {
            if (poll_data_sent())
                can_send = 1;
            if (can_send)
            {
                scanf("%s", shared->send_buffer);
                shared->bytes_to_send = strlen(shared->send_buffer);
                signal_data_ready();
                can_send = 0;
            }
        }
    }
    else
    {
        printf("usage:\n");
        printf("tcpcl 8000: Start a server listening to port 8000\n");
        printf("tcpcl 127 0 0 1 8000: Connect a client to 127.0.0.1:8000\n");
    }
    return 0;
}
