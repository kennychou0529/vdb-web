#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>

#define vdb_work_buffer_size (1024*1024)
#define vdb_recv_buffer_size (1024*1024)
#ifdef VDB_LISTEN_PORT
static int vdb_listen_port = VDB_LISTEN_PORT;
#else
static int vdb_listen_port = 0;
#endif

int vdb_set_listen_port(int port)
{
    #ifdef VDB_LISTEN_PORT
    printf("[vdb] Warning: You are setting the port with vdb_set_listen_port and #define VDB_LISTEN_PORT.\nAre you sure this is intentional?\n");
    #endif
    vdb_listen_port = port;
    return 1;
}

struct vdb_shared_t
{
    int client_socket;
    int listen_socket;
    int has_client_socket;
    int has_listen_socket;
    int has_send_thread;
    int has_recv_thread;
    int send_buffer_busy;
    int work_buffer_used;
    int bytes_to_send;
    char recv_buffer[vdb_recv_buffer_size];
    char swapbuffer1[vdb_work_buffer_size];
    char swapbuffer2[vdb_work_buffer_size];
    char *work_buffer;
    char *send_buffer;
};

static vdb_shared_t *vdb_shared = 0;

int vdb_push_s32(int32_t x)
{
    if (vdb_shared->work_buffer_used + sizeof(x) < vdb_work_buffer_size)
    {
        *(int32_t*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(x);
        return 1;
    }
    return 0;
}

int vdb_push_r32(float x)
{
    if (vdb_shared->work_buffer_used + sizeof(x) < vdb_work_buffer_size)
    {
        *(float*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(x);
        return 1;
    }
    return 0;
}

void vdb_recv_thread()
{
    vdb_shared_t *vs = vdb_shared;
    while (1)
    {
        if (!vs->has_client_socket)
        {
            vdb_log("waiting for a client\n");
            if (!tcp_accept(vs->listen_socket, &vs->client_socket))
            {
                printf("[vdb] Sorry, got a bad client request. Retrying in one second.\n");
                usleep(1000*1000); // retry every one second
                continue;
            }
            vdb_log("Waiting for handshake\n");
            {
                char *response;
                int response_len;
                int received = tcp_recv(vs->client_socket, vs->recv_buffer, vdb_recv_buffer_size);
                if (!received)
                {
                    vdb_log("socket went down on recv thread\n");
                    vs->has_client_socket = 0;
                    tcp_close(vs->client_socket);
                    continue;
                }

                if (!vdb_generate_handshake(vs->recv_buffer, received, &response, &response_len))
                {
                    vdb_log("failed to generate handshake\n");
                    continue;
                }

                if (!tcp_send(vs->client_socket, response, response_len))
                {
                    vdb_log("failed to send handshake\n");
                    continue;
                }
            }
            vdb_log("got a client (%d)\n", vs->client_socket);
            vs->has_client_socket = 1;
        }
        else
        {
            int received = tcp_recv(vs->client_socket, vs->recv_buffer, vdb_recv_buffer_size);
            if (received == 0)
            {
                vdb_log("socket went down on recv thread\n");
                vs->has_client_socket = 0;
                tcp_close(vs->client_socket);
            }
            else if (received > 0)
            {
                vdb_msg_t msg = {0};
                if (vdb_parse_message(vs->recv_buffer, received, &msg))
                {
                    if (!msg.fin)
                    {
                        vdb_log("got an incomplete message (%d): '%s'\n", msg.length, msg.payload);
                    }
                    else
                    {
                        vdb_log("got a message (%d): '%s'\n", msg.length, msg.payload);
                    }
                }
            }
        }
    }
}

void vdb_send_thread()
{
    vdb_shared_t *vs = vdb_shared;
    while (1)
    {
        if (!vs->has_client_socket) // @ UGLY: because file descriptors must be shared
            break;

        if (!vs->send_buffer_busy) // @ INCOMPLETE: SEMAPHORE
        {
            vs->send_buffer_busy = 1;
            if (vs->bytes_to_send > 0)
            {
                unsigned char *frame = 0;
                int frame_len = 0;
                vdb_form_frame(vs->bytes_to_send, &frame, &frame_len);

                if (!tcp_send(vs->client_socket, frame, frame_len))
                {
                    vdb_log("failed to send frame (%s)\n", strerror(errno));
                }

                if (!tcp_send(vs->client_socket, vs->send_buffer, vs->bytes_to_send))
                    vdb_log("failed to send data\n");

                vs->bytes_to_send = 0;
            }
            vs->send_buffer_busy = 0;
        }
        usleep(1000); // @ INCOMPLETE: Block sender on linux
    }
}

int vdb_begin()
{
    if (!vdb_shared)
    {
        vdb_shared = (vdb_shared_t*)mmap(NULL, sizeof(vdb_shared_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (vdb_shared == MAP_FAILED)
        {
            vdb_log("mmap failed\n");
            vdb_shared = 0;
            return 0;
        }
        vdb_shared->work_buffer = vdb_shared->swapbuffer1;
        vdb_shared->send_buffer = vdb_shared->swapbuffer2;
    }

    vdb_shared_t *vs = vdb_shared;

    if (!vs->has_listen_socket)
    {
        if (vdb_listen_port == 0)
        {
            printf("[vdb] Warning: You have not set the listening port for vdb. Using 8000.\n"
                   "You can use a different port by calling vdb_set_listen_port(<port>),\n"
                   "or by #define VDB_LISTEN_PORT <port> before #including vdb.c.\n"
                   "Valid ports are between 1024 and 65535.\n");
            vdb_listen_port = 8000;
        }
        if (vdb_listen_port < 1024 || vdb_listen_port > 65535)
        {
            printf("[vdb] Error: You have requested a vdb_listen_port (%d) outside the valid region of 1024-65535.\n", vdb_listen_port);
            return 0;
        }
        if (!tcp_listen(vdb_listen_port, &vs->listen_socket))
            return 0;
        vs->has_listen_socket = 1;
    }

    if (!vs->has_recv_thread)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            vdb_log("fork failed\n");
            return 0;
        }
        if (pid == 0) // is child
        {
            vdb_recv_thread();
            vs->has_recv_thread = 0;
            _exit(0);
        }
        else
        {
            vs->has_recv_thread = 1;
        }
    }

    if (!vs->has_client_socket)
    {
        return 0;
    }

    #if 1
    if (!vs->has_send_thread)
    {
        vdb_log("creating send thread\n");
        pid_t pid = fork();
        if (pid == -1)
        {
            vdb_log("fork failed\n");
            return 0;
        }
        if (pid == 0) // is child
        {
            vdb_send_thread();
            vs->has_send_thread = 0;
            vdb_log("lost send thread\n");
            _exit(0);
        }
        else
        {
            vs->has_send_thread = 1;
        }
    }
    #endif

    return 1;
}

void vdb_end()
{
    vdb_shared_t *vs = vdb_shared;
    // @ INCOMPLETE: threading
    if (!vs->send_buffer_busy)
    {
        char *new_work_buffer = vs->send_buffer;
        vs->send_buffer = vs->work_buffer;
        vs->work_buffer = new_work_buffer;
        vs->bytes_to_send = vs->work_buffer_used;
        vs->work_buffer_used = 0;
        vs->send_buffer_busy = 0; // @ INCOMPLETE: threading
    }
}

void vdb_sleep(int milliseconds)
{
    usleep(milliseconds*1000);
}
