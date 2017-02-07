#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#define vdb_work_buffer_size (4)
#define vdb_recv_buffer_size (4)
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
    int has_client;
    int has_send_thread;
    int has_recv_thread;
    int work_buffer_used;
    char recv_buffer[vdb_recv_buffer_size];
    char work_buffer[vdb_work_buffer_size];
    char send_buffer[vdb_work_buffer_size];
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
        // if (!vs->has_client)
        // {
        //     if (tcp_accept())
        //         vs->has_client = 1;
        //     else
        //         vdb_logd("tcp_accept failed.\n");
        // }
        printf("hi\n");
        usleep(200*1000);
    }
}

void vdb_send_thread()
{

}

int vdb_begin()
{
    if (!vdb_shared)
    {
        vdb_shared = (vdb_shared_t*)mmap(NULL, sizeof(vdb_shared_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (vdb_shared == MAP_FAILED)
        {
            vdb_logd("mmap failed\n");
            vdb_shared = 0;
            return 0;
        }
        vdb_shared->has_send_thread = 0;
        vdb_shared->has_recv_thread = 0;
        vdb_shared->work_buffer_used = 0;
        vdb_shared->has_client = 0;
    }

    vdb_shared_t *vs = vdb_shared;

    if (!has_listen_socket)
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
        if (!tcp_listen(vdb_listen_port))
            return 0;
    }

    if (!vs->has_recv_thread)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            vdb_logd("fork failed\n");
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

    // if (!vs->has_send_thread)
    // {
    //     pid_t pid = fork();
    //     if (pid == -1)
    //         return 0;
    //     if (pid == 0)
    //     {
    //         vdb_send_thread();
    //         vs->has_send_thread = 0;
    //         _exit(0);
    //     }
    //     vs->has_send_thread = 1;
    // }

    if (!vs->has_client)
    {
        return 0;
    }

    return 1;
}

void vdb_end()
{

}

void vdb_sleep(int milliseconds)
{
    usleep(milliseconds*1000);
}
