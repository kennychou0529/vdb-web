#define vdb_assert(EXPR) if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; }
#include "tcp.c"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/mman.h>

struct shared_t
{
    char send_buffer[1024*1024];
    int bytes_to_send;
    int has_send_thread;
};

static shared_t *shared = 0;

void send_thread()
{
    int sent_bytes = 0;
    char *send_ptr = shared->send_buffer;
    while (1)
    {
        if (shared->bytes_to_send > 0) // @ RACECOND: SHARED MUTEX
        {
            if (!tcp_send(send_ptr, shared->bytes_to_send, &sent_bytes))
            {
                break;
            }
            send_ptr += sent_bytes;
            shared->bytes_to_send -= sent_bytes;
            if (shared->bytes_to_send == 0)
                send_ptr = shared->send_buffer;
        }
        usleep(1000*1000); // todo: do a semaphore here!
    }
}

void read_thread(int port)
{
    static char read_buffer[1024*1024] = {0};
    int read_bytes = 0;
    pid_t send_pid = 0;
    while (1)
    {
        if (!has_listen_socket)
        {
            printf("creating listen socket\n");
            if (!tcp_listen(port))
            {
                usleep(1000*1000); // Try again in one second
                continue;
            }
        }
        if (!has_client_socket)
        {
            printf("waiting for client\n");
            if (!tcp_accept())
            {
                usleep(1000*1000); // Try again in one second
                continue;
            }
        }
        if (!shared->has_send_thread) // @ RACECOND: MUTEX ON SHARED
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
        shared = (shared_t*)mmap(NULL, sizeof(shared_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (shared == MAP_FAILED)
            return 0;

        int port = atoi(argv[1]);
        pid_t pid = fork();
        if (pid == -1) { return 0; }
        if (pid == 0)  { read_thread(port); _exit(0); }

        while (1)
        {
            scanf("%s", shared->send_buffer);
            shared->bytes_to_send = strlen(shared->send_buffer); // @ RACECOND: MUTEX ON SHARED
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
