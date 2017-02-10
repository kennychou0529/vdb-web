// Notes to future self:
// * The send thread is allowed to return on unix, because if the connection
// goes down, the recv thread needs to respawn the process after a new
// client connection has been acquired (to share the file descriptor).
// fork() shares the open file descriptors with the child process, but
// if a file descriptor is _then_ opened in the parent, it will _not_
// be shared with the child.

#if defined(_WIN32) || defined(_WIN64)
#define VDB_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#else
#define VDB_UNIX
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#define vdb_assert(EXPR)  { if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; } }
#define vdb_log(...)      { printf("[vdb] %s@L%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); }
#define vdb_log_once(...) { static int first = 1; if (first) { printf("[vdb] "); printf(__VA_ARGS__); first = 0; } }
#define vdb_err_once(...) { static int first = 1; if (first) { printf("[vdb] Error at line %d in file %s:\n[vdb] ", __LINE__, __FILE__); printf(__VA_ARGS__); first = 0; } }
#define vdb_critical(EXPR) if (!(EXPR)) { printf("[vdb] Something went wrong at line %d in file %s\n", __LINE__, __FILE__); vdb_shared->critical_error = 1; return 0; }

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tcp.c"
#include "websocket.c"

#ifndef VDB_WORK_BUFFER_SIZE
#define VDB_WORK_BUFFER_SIZE (1024*1024)
#endif

#ifndef VDB_RECV_BUFFER_SIZE
#define VDB_RECV_BUFFER_SIZE (1024*1024)
#endif

#ifndef VDB_LISTEN_PORT
#define VDB_LISTEN_PORT 8000
#define VDB_USING_DEFAULT_LISTEN_PORT
#endif

struct vdb_shared_t
{
    #ifdef VDB_WINDOWS
    volatile HANDLE send_semaphore;
    volatile LONG busy;
    volatile int bytes_to_send;
    #else
    int bytes_to_send;
    pid_t send_pid;
    int has_send_thread;
    // These pipes are used for flow control between the main thread and the sending thread
    // The sending thread blocks on a read on pipe_ready, until the main thread signals the
    // pipe by write on pipe_ready. The main thread checks if sending is complete by polling
    // (non-blocking) pipe_done, which is signalled by the sending thread.
    int ready[2]; // [0]: read, [1]: send
    int done[2];// [0]: read, [1]: send
    #endif

    int critical_error;
    int has_connection;
    int work_buffer_used;
    char recv_buffer[VDB_RECV_BUFFER_SIZE];
    char swapbuffer1[VDB_WORK_BUFFER_SIZE];
    char swapbuffer2[VDB_WORK_BUFFER_SIZE];
    char *work_buffer;
    char *send_buffer;
};

static vdb_shared_t *vdb_shared = 0;
static int vdb_listen_port = VDB_LISTEN_PORT;

int vdb_set_listen_port(int port)
{
    #ifndef VDB_USING_DEFAULT_LISTEN_PORT
    printf("[vdb] Warning: You are setting the port with both vdb_set_listen_port and #define VDB_LISTEN_PORT.\nAre you sure this is intentional?\n");
    #endif
    vdb_listen_port = port;
    return 1;
}

#ifdef VDB_WINDOWS
int vdb_wait_data_ready()
{
    WaitForSingleObject(vdb_shared->send_semaphore, INFINITE);
    while (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 1)
    {
        vdb_log("CompareExchange blocked\n");
    }
    return 1;
}
int vdb_signal_data_sent()  { vdb_shared->busy = 0; return 1; }
int vdb_poll_data_sent()    { return (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 0); }
int vdb_signal_data_ready() { vdb_shared->busy = 0; ReleaseSemaphore(vdb_shared->send_semaphore, 1, 0); return 1; } // @ mfence, writefence
void vdb_sleep(int ms)      { Sleep(ms); }
#else
int vdb_wait_data_ready()   { int val = 0; return  read(vdb_shared->ready[0], &val, sizeof(val)) == sizeof(val); }
int vdb_poll_data_sent()    { int val = 0; return   read(vdb_shared->done[0], &val, sizeof(val)) == sizeof(val); }
int vdb_signal_data_ready() { int one = 1; return write(vdb_shared->ready[1], &one, sizeof(one)) == sizeof(one); }
int vdb_signal_data_sent()  { int one = 1; return  write(vdb_shared->done[1], &one, sizeof(one)) == sizeof(one); }
void vdb_sleep(int ms)      { usleep(ms*1000); }
#endif

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_send_thread(void *)
#else
int vdb_send_thread()
#endif
{
    vdb_shared_t *vs = vdb_shared;
    unsigned char *frame; // @ UGLY: form_frame should modify a char *?
    int frame_len;
    vdb_log("Created send thread\n");
    while (!vs->critical_error)
    {
        // blocking until data is signalled ready from main thread
        vdb_critical(vdb_wait_data_ready());

        // send frame header
        vdb_form_frame(vs->bytes_to_send, &frame, &frame_len);
        if (!tcp_sendall(frame, frame_len))
        {
            vdb_log("failed to send frame\n");
            vdb_critical(vdb_signal_data_sent());
            return 0;
        }

        // send the payload
        if (!tcp_sendall(vs->send_buffer, vs->bytes_to_send))
        {
            vdb_log("failed to send payload\n");
            vdb_critical(vdb_signal_data_sent());
            return 0;
        }

        // signal to main thread that data has been sent
        vdb_critical(vdb_signal_data_sent());
    }
    return 1;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_recv_thread(void *)
#else
int vdb_recv_thread()
#endif
{
    vdb_shared_t *vs = vdb_shared;
    vdb_log("Created read thread\n");
    int read_bytes;
    vdb_msg_t msg;
    while (!vs->critical_error)
    {
        if (vs->critical_error)
        {
            return 0;
        }
        if (!has_listen_socket)
        {
            if (vdb_listen_port == 0)
            {
                vdb_log_once("You have not set the listening port for vdb. Using 8000.\n"
                       "You can use a different port by calling vdb_set_listen_port(<port>),\n"
                       "or by #define VDB_LISTEN_PORT <port> before #including vdb.c.\n"
                       "Valid ports are between 1024 and 65535.\n");
                vdb_listen_port = 8000;
            }
            vdb_log("Creating listen socket\n");
            if (!tcp_listen(vdb_listen_port))
            {
                vdb_log("listen failed\n");
                vdb_sleep(1000);
                continue;
            }
            vdb_log_once("Visualization is live at <Your IP>:%d\n", vdb_listen_port);
        }
        if (!has_client_socket)
        {
            vdb_log("Waiting for client\n");
            if (!tcp_accept())
            {
                vdb_sleep(1000);
                continue;
            }
        }
        if (!vs->has_connection)
        {
            char *response;
            int response_len;
            vdb_log("Waiting for handshake\n");
            if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes))
            {
                vdb_log("lost connection during handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vdb_log("Generating handshake\n");
            if (!vdb_generate_handshake(vs->recv_buffer, read_bytes, &response, &response_len))
            {
                vdb_log("Failed to generate handshake\n");

                // If we failed to generate a handshake, it means that either
                // we did something wrong, or the browser did something wrong,
                // or the request was an ordinary HTTP request. If the latter
                // we'll send an HTTP response containing the vdb.html page.
                char data[1024];
                const char *content = "<html>In the future I'll also send the vdb application to you over the browser.<br>For now, you need to open the app.html file in your browser.</html>";
                int len = sprintf(data,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n%s",
                    (int)strlen(content),
                    content
                    );

                if (!tcp_sendall(data, len))
                {
                    vdb_log("Lost connection while sending HTML page\n");
                    tcp_shutdown();
                    vdb_sleep(1000);
                    continue;
                }

                // Sending 'Connection: Closed' allows us to close the socket immediately
                tcp_close_client();

                vdb_sleep(1000);
                continue;
            }

            vdb_log("Sending handshake\n");
            if (!tcp_sendall(response, response_len))
            {
                vdb_log("Failed to send handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vs->has_connection = 1;
        }
        #ifdef VDB_UNIX
        if (!vs->has_send_thread) // @ RACECOND: UGLY: Spawn only one sending thread!!!
        {
            vs->send_pid = fork();
            vdb_critical(vs->send_pid != -1);
            if (vs->send_pid == 0)
            {
                vs->has_send_thread = 1;
                vdb_send_thread();
                vs->has_send_thread = 0;
                _exit(0);
            }
            vdb_sleep(100); // @ RACECOND: UGLY: Let child process set has_send_thread = 1
        }
        #endif
        if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes)) // @ INCOMPLETE: Assemble frames
        {
            vdb_log("Connection went down\n");
            vs->has_connection = 0;
            tcp_shutdown();
            #ifdef VDB_UNIX
            if (vs->has_send_thread)
            {
                kill(vs->send_pid, SIGUSR1);
                vs->has_send_thread = 0;
            }
            #endif
            vdb_sleep(100);
            continue;
        }
        if (!vdb_parse_message(vs->recv_buffer, read_bytes, &msg))
        {
            vdb_log("Got a bad message\n");
            continue;
        }
        if (!msg.fin)
        {
            vdb_log("Got an incomplete message (%d): '%s'\n", msg.length, msg.payload);
            continue;
        }
        vdb_log("Got a final message (%d): '%s'\n", msg.length, msg.payload);
    }
    return 0;
}

void vdb_initialize_cmdbuf(); // Forward-declare (see below)
int vdb_serialize_cmdbuf(); // Forward-declare (see below)

int vdb_begin()
{
    if (vdb_listen_port < 1024 || vdb_listen_port > 65535)
    {
        vdb_err_once("You have requested a vdb_listen_port (%d) outside the valid range 1024-65535.\n", vdb_listen_port);
        return 0;
    }
    if (!vdb_shared)
    {
        #ifdef VDB_UNIX
        // Share memory between main process and child processes
        vdb_shared = (vdb_shared_t*)mmap(NULL, sizeof(vdb_shared_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (vdb_shared == MAP_FAILED)
        {
            vdb_err_once("Tried to allocate too much memory, try lowering VDB_RECV_BUFFER_SIZE and VDB_SEND_BUFFER_SIZE\n");
            vdb_shared = 0;
            vdb_shared->critical_error = 1;
            return 0;
        }
        vdb_critical(pipe(vdb_shared->ready) != -1);
        vdb_critical(pipe2(vdb_shared->done, O_NONBLOCK) != -1);
        vdb_signal_data_sent(); // Needed for first vdb_end call
        // Create recv_thread
        {
            pid_t pid = fork();
            vdb_critical(pid != -1);
            if (pid == 0) { vdb_recv_thread(); _exit(0); }
        }
        #else
        vdb_shared = (vdb_shared_t*)calloc(sizeof(vdb_shared_t),1); // zero-initialize
        if (!vdb_shared)
        {
            vdb_err_once("Tried to allocate too much memory, try lowering VDB_RECV_BUFFER_SIZE and VDB_SEND_BUFFER_SIZE.\n");
            vdb_shared->critical_error = 1;
            return 0;
        }
        vdb_shared->send_semaphore = CreateSemaphore(0, 0, 1, 0);
        CreateThread(0, 0, vdb_recv_thread, NULL, 0, 0);
        CreateThread(0, 0, vdb_send_thread, NULL, 0, 0);
        #endif

        vdb_shared->work_buffer = vdb_shared->swapbuffer1;
        vdb_shared->send_buffer = vdb_shared->swapbuffer2;
        // remaining parameters are zero-initialized by mmap
    }
    if (vdb_shared->critical_error)
    {
        vdb_err_once("You must restart your program to use vdb.\n");
        return 0;
    }
    if (!vdb_shared->has_connection)
    {
        return 0;
    }
    vdb_initialize_cmdbuf();
    return 1;
}

void vdb_end()
{
    vdb_shared_t *vs = vdb_shared;
    if (vdb_poll_data_sent()) // Check if send_thread has finished sending data
    {
        vs->work_buffer_used = 0;
        if (!vdb_serialize_cmdbuf())
        {
            vdb_log_once("Too much geometry was drawn. Try increasing the work buffer size.\n");
            vs->work_buffer_used = 0;
        }

        char *new_work_buffer = vs->send_buffer;
        vs->send_buffer = vs->work_buffer;
        vs->bytes_to_send = vs->work_buffer_used;
        vs->work_buffer = new_work_buffer;
        vs->work_buffer_used = 0;

        // Notify sending thread that data is available
        vdb_signal_data_ready();
    }
}

// Pushbuffer API implementation

void *vdb_push_bytes(const void *data, int count)
{
    if (vdb_shared->work_buffer_used + count <= VDB_WORK_BUFFER_SIZE)
    {
        const char *src = (const char*)data;
              char *dst = vdb_shared->work_buffer + vdb_shared->work_buffer_used;
        if (src) memcpy(dst, src, count);
        else     memset(dst, 0, count);
        vdb_shared->work_buffer_used += count;
        return (void*)dst;
    }
    return 0;
}

int vdb_push_u08(uint8_t x)
{
    if (vdb_shared->work_buffer_used + sizeof(uint8_t) <= VDB_WORK_BUFFER_SIZE)
    {
        *(uint8_t*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(uint8_t);
        return 1;
    }
    return 0;
}

int vdb_push_u32(uint32_t x)
{
    if (vdb_shared->work_buffer_used + sizeof(uint32_t) <= VDB_WORK_BUFFER_SIZE)
    {
        *(uint32_t*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(uint32_t);
        return 1;
    }
    return 0;
}

int vdb_push_r32(float x)
{
    if (vdb_shared->work_buffer_used + sizeof(float) <= VDB_WORK_BUFFER_SIZE)
    {
        *(float*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(float);
        return 1;
    }
    return 0;
}

// Public API implementation

#define VDB_NUM_POINT2 (1024*100)
#define VDB_NUM_POINT3 (1024*100)
#define VDB_NUM_LINE2 (1024*100)
#define VDB_NUM_LINE3 (1024*100)
struct vdb_point2_t { uint8_t c; float x,y; };
struct vdb_point3_t { uint8_t c; float x,y,z; };
struct vdb_line2_t  { uint8_t c; float x1,y1,x2,y2; };
struct vdb_line3_t  { uint8_t c; float x1,y1,z1,x2,y2,z2; };
struct vdb_cmdbuf_t
{
    uint8_t color;
    uint32_t num_line2;
    uint32_t num_line3;
    uint32_t num_point2;
    uint32_t num_point3;
    vdb_line2_t  line2[VDB_NUM_LINE2];
    vdb_line3_t  line3[VDB_NUM_LINE3];
    vdb_point2_t point2[VDB_NUM_POINT2];
    vdb_point3_t point3[VDB_NUM_POINT3];
};
static vdb_cmdbuf_t vdb_cmdbuf_static = {0}; // @ Might fail if too big; use calloc
static vdb_cmdbuf_t *vdb_cmdbuf = &vdb_cmdbuf_static;

void vdb_initialize_cmdbuf()
{
    vdb_cmdbuf->num_line2 = 0;
    vdb_cmdbuf->num_line3 = 0;
    vdb_cmdbuf->num_point2 = 0;
    vdb_cmdbuf->num_point3 = 0;
    // @ command buffer initialization
    vdb_cmdbuf->color = 0;
}

int vdb_serialize_cmdbuf()
{
    vdb_cmdbuf_t *b = vdb_cmdbuf;
    if (!vdb_push_u32(b->num_line2))  return 0;
    if (!vdb_push_u32(b->num_line3))  return 0;
    if (!vdb_push_u32(b->num_point2)) return 0;
    if (!vdb_push_u32(b->num_point3)) return 0;
    for (uint32_t i = 0; i < b->num_line2; i++)
    {
        if (!vdb_push_u08(b->line2[i].c))  return 0;
        if (!vdb_push_r32(b->line2[i].x1)) return 0;
        if (!vdb_push_r32(b->line2[i].y1)) return 0;
        if (!vdb_push_r32(b->line2[i].x2)) return 0;
        if (!vdb_push_r32(b->line2[i].y2)) return 0;
    }
    for (uint32_t i = 0; i < b->num_line3; i++)
    {
        if (!vdb_push_u08(b->line3[i].c))  return 0;
        if (!vdb_push_r32(b->line3[i].x1)) return 0;
        if (!vdb_push_r32(b->line3[i].y1)) return 0;
        if (!vdb_push_r32(b->line3[i].z1)) return 0;
        if (!vdb_push_r32(b->line3[i].x2)) return 0;
        if (!vdb_push_r32(b->line3[i].y2)) return 0;
        if (!vdb_push_r32(b->line3[i].z2)) return 0;
    }
    for (uint32_t i = 0; i < b->num_point2; i++)
    {
        if (!vdb_push_u08(b->point2[i].c)) return 0;
        if (!vdb_push_r32(b->point2[i].x)) return 0;
        if (!vdb_push_r32(b->point2[i].y)) return 0;
    }
    for (uint32_t i = 0; i < b->num_point3; i++)
    {
        if (!vdb_push_u08(b->point3[i].c)) return 0;
        if (!vdb_push_r32(b->point3[i].x)) return 0;
        if (!vdb_push_r32(b->point3[i].y)) return 0;
    }

    return 1;
}

void vdb_color1i(int c)
{
    if (c < 0) c = 0;
    if (c > 255) c = 255;
    vdb_cmdbuf->color = (unsigned char)(c);
}

void vdb_color1f(float c)
{
    int ci = (int)(c*255.0f);
    if (ci < 0) ci = 0;
    if (ci > 255) ci = 255;
    vdb_cmdbuf->color = (unsigned char)(ci);
}

void vdb_point2(float x, float y)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_point2 < VDB_NUM_POINT2)
    {
        vdb_point2_t *p = &vdb_cmdbuf->point2[vdb_cmdbuf->num_point2];
        p->c = vdb_cmdbuf->color;
        p->x = x;
        p->y = y;
        vdb_cmdbuf->num_point2++;
    }
}

void vdb_point3(float x, float y, float z)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_point3 < VDB_NUM_POINT3)
    {
        vdb_point3_t *p = &vdb_cmdbuf->point3[vdb_cmdbuf->num_point3];
        p->c = vdb_cmdbuf->color;
        p->x = x;
        p->y = y;
        p->z = z;
        vdb_cmdbuf->num_point3++;
    }
}

void vdb_line2(float x1, float y1, float x2, float y2)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_line2 < VDB_NUM_LINE2)
    {
        vdb_line2_t *p = &vdb_cmdbuf->line2[vdb_cmdbuf->num_line2];
        p->c = vdb_cmdbuf->color;
        p->x1 = x1;
        p->y1 = y1;
        p->x2 = x2;
        p->y2 = y2;
        vdb_cmdbuf->num_line2++;
    }
}

void vdb_line3(float x1, float y1, float z1, float x2, float y2, float z2)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_line3 < VDB_NUM_LINE3)
    {
        vdb_line3_t *p = &vdb_cmdbuf->line3[vdb_cmdbuf->num_line3];
        p->c = vdb_cmdbuf->color;
        p->x1 = x1;
        p->y1 = y1;
        p->z1 = z1;
        p->x2 = x2;
        p->y2 = y2;
        p->z2 = z2;
        vdb_cmdbuf->num_line3++;
    }
}
