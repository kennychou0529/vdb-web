// implementation

#define vdb_assert(EXPR)  { if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; } }
#ifdef VDB_LOG_DEBUG
#define vdb_log(...)      { printf("[vdb] %s@L%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); }
#else
#define vdb_log(...)      { }
#endif
#define vdb_log_once(...) { static int first = 1; if (first) { printf("[vdb] "); printf(__VA_ARGS__); first = 0; } }
#define vdb_err_once(...) { static int first = 1; if (first) { printf("[vdb] Error at line %d in file %s:\n[vdb] ", __LINE__, __FILE__); printf(__VA_ARGS__); first = 0; } }
#define vdb_critical(EXPR) if (!(EXPR)) { printf("[vdb] Something went wrong at line %d in file %s\n", __LINE__, __FILE__); vdb_shared->critical_error = 1; return 0; }

#if defined(_WIN32) || defined(_WIN64)
#define VDB_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define VDB_UNIX
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tcp.c"
#include "websocket.c"

// Draw commands are stored in a work buffer that is allocated
// once on the first vdb_begin call, and stays a fixed size that
// is given below in number-of-bytes. If you are memory constrained,
// or if you need more memory than allocated by default, you can
// define your own work buffer size before #including vdb.
#ifndef VDB_WORK_BUFFER_SIZE
#define VDB_WORK_BUFFER_SIZE (32*1024*1024)
#endif

// Messages received from the browser are stored in a buffer that
// is allocated once on the first vdb_begin call, and stays a fixed
// size is given below in number-of-bytes. If you are sending large
// messages from the browser back to the application you can define
// your own recv buffer size before #including vdb.
#ifndef VDB_RECV_BUFFER_SIZE
#define VDB_RECV_BUFFER_SIZE (1024*1024)
#endif

#ifndef VDB_LISTEN_PORT
#define VDB_LISTEN_PORT 8000
#define VDB_USING_DEFAULT_LISTEN_PORT
#endif

#define VDB_MAX_S32_VARIABLES 1024
#define VDB_MAX_R32_VARIABLES 1024

#define VDB_LABEL_LENGTH 16
typedef struct
{
    char chars[VDB_LABEL_LENGTH+1];
} vdb_label_t;

int vdb_cmp_label(vdb_label_t *a, vdb_label_t *b)
{
    int i;
    for (i = 0; i < VDB_LABEL_LENGTH; i++)
        if (a->chars[i] != b->chars[i])
            return 0;
    return 1;
}

void vdb_copy_label(vdb_label_t *dst, const char *src)
{
    int i = 0;
    while (i < VDB_LABEL_LENGTH && src[i])
    {
        dst->chars[i] = src[i];
        i++;
    }
    while (i < VDB_LABEL_LENGTH)
    {
        dst->chars[i] = ' ';
        i++;
    }
    dst->chars[VDB_LABEL_LENGTH] = 0;
}

typedef struct
{
    #ifdef VDB_WINDOWS
    volatile HANDLE send_semaphore;
    volatile LONG busy;
    volatile int bytes_to_send;
    #else
    int bytes_to_send;
    pid_t send_pid;
    // These pipes are used for flow control between the main thread and the sending thread
    // The sending thread blocks on a read on pipe_ready, until the main thread signals the
    // pipe by write on pipe_ready. The main thread checks if sending is complete by polling
    // (non-blocking) pipe_done, which is signalled by the sending thread.
    int ready[2]; // [0]: read, [1]: send
    int done[2];// [0]: read, [1]: send
    #endif

    int has_send_thread;
    int critical_error;
    int has_connection;
    int work_buffer_used;
    char swapbuffer1[VDB_WORK_BUFFER_SIZE];
    char swapbuffer2[VDB_WORK_BUFFER_SIZE];
    char *work_buffer;
    char *send_buffer;

    char recv_buffer[VDB_RECV_BUFFER_SIZE];

    // These hold the latest state from the browser
    vdb_label_t msg_var_r32_label[VDB_MAX_R32_VARIABLES];
    float       msg_var_r32_value[VDB_MAX_R32_VARIABLES];
    int         msg_var_r32_count;
    int         msg_flag_continue;
    // float    app_mouse_x;
    // float    app_mouse_y;
} vdb_shared_t;

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

int vdb_send_thread()
{
    vdb_shared_t *vs = vdb_shared;
    unsigned char *frame; // @ UGLY: form_frame should modify a char *?
    int frame_len;
    vdb_log("Created send thread\n");
    vdb_sleep(100); // @ RACECOND: Let the parent thread set has_send_thread to 1
    while (!vs->critical_error)
    {
        // blocking until data is signalled ready from main thread
        vdb_critical(vdb_wait_data_ready());

        // send frame header
        vdb_form_frame(vs->bytes_to_send, &frame, &frame_len);
        if (!tcp_sendall(frame, frame_len))
        {
            vdb_log("Failed to send frame\n");
            vdb_critical(vdb_signal_data_sent());
            break;
        }

        // send the payload
        if (!tcp_sendall(vs->send_buffer, vs->bytes_to_send))
        {
            vdb_log("Failed to send payload\n");
            vdb_critical(vdb_signal_data_sent());
            break;
        }

        // signal to main thread that data has been sent
        vdb_critical(vdb_signal_data_sent());
    }
    vs->has_send_thread = 0;
    return 0;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_win_send_thread(void *vdata) { (void)(vdata); return vdb_send_thread(); }
#endif

int vdb_handle_message(vdb_msg_t msg)
{
    // vdb_log("Got a message (%d): '%s'\n", msg.length, msg.payload);
    vdb_shared_t *vs = vdb_shared;

    if (msg.length == 1 && msg.payload[0] == 'c') // asynchronous 'continue' event
    {
        vs->msg_flag_continue = 1;
        return 1;
    }

    // @ todo: mutex on latest message?
    // status format:
    // 's int char[16] float char[16] float ... char[16] float'
    if (msg.length > 1 && msg.payload[0] == 's') // status update
    {
        char *str = msg.payload;
        int pos = 0 + 2;
        int got = 0;
        {
            int i = 0;
            int num_vars;
            vdb_assert(sscanf(str+pos, "%d%n", &num_vars, &got) == 1);
            vdb_assert(num_vars >= 0 && num_vars < VDB_MAX_R32_VARIABLES);
            if (num_vars == 0)
                return 1;

            pos += got; // read past int
            pos +=   1; // read past space
            vdb_assert(pos < msg.length);

            for (i = 0; i < num_vars; i++)
            {
                vdb_label_t label = {0};
                float value = 0.0f;

                // read label
                vdb_assert(pos + VDB_LABEL_LENGTH < msg.length);
                vdb_copy_label(&label, str+pos);
                pos += VDB_LABEL_LENGTH;

                // read value
                vdb_assert(sscanf(str+pos, "%f%n", &value, &got) == 1);

                pos += got; // read past float
                pos +=   1; // read past space

                vs->msg_var_r32_label[i] = label;
                vs->msg_var_r32_value[i] = value;
            }
            vs->msg_var_r32_count = num_vars;
        }
    }

    return 1;
}

int vdb_recv_thread()
{
    vdb_shared_t *vs = vdb_shared;
    int read_bytes;
    vdb_msg_t msg;
    vdb_log("Created read thread\n");
    while (!vs->critical_error)
    {
        if (vs->critical_error)
        {
            return 0;
        }
        if (!tcp_has_listen_socket)
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
        if (!tcp_has_client_socket)
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
                static char http_response[1024*1024];
                const char *content = "<html>In the future I'll also send the vdb application to you over the browser.<br>For now, you need to open the app.html file in your browser.</html>";

                // If we failed to generate a handshake, it means that either
                // we did something wrong, or the browser did something wrong,
                // or the request was an ordinary HTTP request. If the latter
                // we'll send an HTTP response containing the vdb.html page.
                int len = sprintf(http_response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n%s",
                    (int)strlen(content),
                    content
                    );

                vdb_log("Failed to generate handshake. Sending HTML page.\n");
                if (!tcp_sendall(http_response, len))
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
        // The send thread is allowed to return on unix, because if the connection
        // goes down, the recv thread needs to respawn the process after a new
        // client connection has been acquired (to share the file descriptor).
        // fork() shares the open file descriptors with the child process, but
        // if a file descriptor is _then_ opened in the parent, it will _not_
        // be shared with the child.
        if (!vs->has_send_thread)
        {
            vs->send_pid = fork();
            vdb_critical(vs->send_pid != -1);
            if (vs->send_pid == 0)
            {
                vdb_send_thread(); // vdb_send_thread sets has_send_thread to 0 upon returning
                _exit(0);
            }
            vs->has_send_thread = 1;
        }
        #else
        // Because we allow it to return on unix, we allow it to return on windows
        // as well, even though file descriptors are shared anyway.
        if (!vs->has_send_thread)
        {
            CreateThread(0, 0, vdb_win_send_thread, NULL, 0, 0); // vdb_send_thread sets has_send_thread to 0 upon returning
            vs->has_send_thread = 1;
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
        vdb_handle_message(msg);
    }
    return 0;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_win_recv_thread(void *vdata) { (void)(vdata); return vdb_recv_thread(); }
#endif

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
    return NULL;
}

#define _vdb_push_type(VALUE, TYPE)                                                  \
    if (vdb_shared->work_buffer_used + sizeof(TYPE) <= VDB_WORK_BUFFER_SIZE)         \
    {                                                                                \
        TYPE *ptr = (TYPE*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used); \
        vdb_shared->work_buffer_used += sizeof(TYPE);                                \
        *ptr = VALUE;                                                                \
        return ptr;                                                                  \
    }                                                                                \
    return NULL;

uint8_t  *vdb_push_u08(uint8_t x)  { _vdb_push_type(x, uint8_t);  }
uint32_t *vdb_push_u32(uint32_t x) { _vdb_push_type(x, uint32_t); }
float    *vdb_push_r32(float x)    { _vdb_push_type(x, float);    }

void vdb_init_drawstate();

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
        vdb_shared = (vdb_shared_t*)mmap(NULL, sizeof(vdb_shared_t),
                                         PROT_READ|PROT_WRITE,
                                         MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (vdb_shared == MAP_FAILED)
            vdb_shared = 0;
        #else
        vdb_shared = (vdb_shared_t*)calloc(sizeof(vdb_shared_t),1);
        #endif

        if (!vdb_shared)
        {
            vdb_err_once("Tried to allocate too much memory, try lowering VDB_RECV_BUFFER_SIZE and VDB_SEND_BUFFER_SIZE\n");
            vdb_shared->critical_error = 1;
            return 0;
        }

        #ifdef VDB_UNIX
        vdb_critical(pipe(vdb_shared->ready) != -1);
        vdb_critical(pipe2(vdb_shared->done, O_NONBLOCK) != -1);
        {
            pid_t pid = fork();
            vdb_critical(pid != -1);
            if (pid == 0) { vdb_recv_thread(); _exit(0); }
        }
        vdb_signal_data_sent(); // Needed for first vdb_end call
        #else
        vdb_shared->send_semaphore = CreateSemaphore(0, 0, 1, 0);
        CreateThread(0, 0, vdb_win_recv_thread, NULL, 0, 0);
        #endif

        vdb_shared->work_buffer = vdb_shared->swapbuffer1;
        vdb_shared->send_buffer = vdb_shared->swapbuffer2;
        // Remaining parameters should be initialized to zero by calloc or mmap
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
    vdb_shared->work_buffer_used = 0;
    vdb_init_drawstate();
    return 1;
}

void vdb_end()
{
    vdb_shared_t *vs = vdb_shared;
    if (vdb_poll_data_sent()) // Check if send_thread has finished sending data
    {
        char *new_work_buffer = vs->send_buffer;
        vs->send_buffer = vs->work_buffer;
        vs->bytes_to_send = vs->work_buffer_used;
        vs->work_buffer = new_work_buffer;
        vs->work_buffer_used = 0;

        // Notify sending thread that data is available
        vdb_signal_data_ready();
    }
}

int vdb_loop(int fps)
{
    static int entry = 1;
    if (entry)
    {
        while (!vdb_begin())
        {
        }
        entry = 0;
    }
    else
    {
        vdb_end();
        vdb_sleep(1000/fps);
        if (vdb_shared->msg_flag_continue)
        {
            vdb_shared->msg_flag_continue = 0;
            entry = 1;
            return 0;
        }
        while (!vdb_begin())
        {
        }
    }
    return 1;
}

// Public API implementation

#define vdb_color_mode_primary  0
#define vdb_color_mode_ramp     1

#define vdb_mode_point2         1
#define vdb_mode_point3         2
#define vdb_mode_line2          3
#define vdb_mode_line3          4
#define vdb_mode_fill_rect      5
#define vdb_mode_circle         6
#define vdb_mode_image_rgb8     7
#define vdb_mode_slider_float   254

static unsigned char vdb_current_color_mode = 0;
static unsigned char vdb_current_color = 0;
static unsigned char vdb_current_alpha = 0;

static float vdb_xrange_left = -1.0f;
static float vdb_xrange_right = +1.0f;
static float vdb_yrange_bottom = -1.0f;
static float vdb_yrange_top = +1.0f;
static float vdb_zrange_far = -1.0f;
static float vdb_zrange_near = +1.0f;

void vdb_init_drawstate()
{
    vdb_current_color_mode = vdb_color_mode_primary;
    vdb_current_alpha = 0;
    vdb_xrange_left = -1.0f;
    vdb_xrange_right = +1.0f;
    vdb_yrange_bottom = -1.0f;
    vdb_yrange_top = +1.0f;
    vdb_zrange_far = -1.0f;
    vdb_zrange_near = +1.0f;
}

void vdb_translucent() { vdb_current_alpha = 1; }
void vdb_opaque()      { vdb_current_alpha = 0; }

void vdb_color_primary(int primary, int shade)
{
    if (primary < 0) primary = 0;
    if (primary > 4) primary = 4;
    if (shade < 0) shade = 0;
    if (shade > 2) shade = 2;
    vdb_current_color_mode = vdb_color_mode_primary;
    vdb_current_color = (unsigned char)(3*primary + shade);
}

void vdb_color_rampf(float value)
{
    int i = (int)(value*63.0f);
    if (i < 0) i = 0;
    if (i > 63) i = 63;
    vdb_current_color_mode = vdb_color_mode_ramp;
    vdb_current_color = (unsigned char)i;
}

void vdb_color_ramp(int i)
{
    vdb_current_color_mode = vdb_color_mode_ramp;
    vdb_current_color = (unsigned char)(i % 63);
}

void vdb_color_red(int shade)   { vdb_color_primary(0, shade); }
void vdb_color_green(int shade) { vdb_color_primary(1, shade); }
void vdb_color_blue(int shade)  { vdb_color_primary(2, shade); }
void vdb_color_black(int shade) { vdb_color_primary(3, shade); }
void vdb_color_white(int shade) { vdb_color_primary(4, shade); }

void vdb_xrange(float left, float right)
{
    vdb_xrange_left = left;
    vdb_xrange_right = right;
}

void vdb_yrange(float bottom, float top)
{
    vdb_yrange_bottom = bottom;
    vdb_yrange_top = top;
}

void vdb_zrange(float z_near, float z_far)
{
    vdb_zrange_near = z_near;
    vdb_zrange_far = z_far;
}

float vdb_map_x(float x) { return -1.0f + 2.0f*(x-vdb_xrange_left)/(vdb_xrange_right-vdb_xrange_left); }
float vdb_map_y(float y) { return -1.0f + 2.0f*(y-vdb_yrange_bottom)/(vdb_yrange_top-vdb_yrange_bottom); }
float vdb_map_z(float z) { return +1.0f - 2.0f*(z-vdb_zrange_near)/(vdb_zrange_far-vdb_zrange_near); }

void vdb_push_style()
{
    unsigned char opacity = ((vdb_current_alpha & 0x01)      << 7);
    unsigned char mode    = ((vdb_current_color_mode & 0x01) << 6);
    unsigned char value   = ((vdb_current_color & 0x3F)      << 0);
    unsigned char style   = opacity | mode | value;
    vdb_push_u08(style);
}

void vdb_point(float x, float y)
{
    vdb_push_u08(vdb_mode_point2);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
}

void vdb_point3d(float x, float y, float z)
{
    vdb_push_u08(vdb_mode_point3);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
    vdb_push_r32(vdb_map_z(z));
}

void vdb_line(float x1, float y1, float x2, float y2)
{
    vdb_push_u08(vdb_mode_line2);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x1));
    vdb_push_r32(vdb_map_y(y1));
    vdb_push_r32(vdb_map_x(x2));
    vdb_push_r32(vdb_map_y(y2));
}

void vdb_line3d(float x1, float y1, float z1, float x2, float y2, float z2)
{
    vdb_push_u08(vdb_mode_line3);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x1));
    vdb_push_r32(vdb_map_y(y1));
    vdb_push_r32(vdb_map_z(z1));
    vdb_push_r32(vdb_map_x(x2));
    vdb_push_r32(vdb_map_y(y2));
    vdb_push_r32(vdb_map_z(z2));
}

void vdb_fillRect(float x, float y, float w, float h)
{
    vdb_push_u08(vdb_mode_fill_rect);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
    vdb_push_r32(vdb_map_x(x+w));
    vdb_push_r32(vdb_map_y(y+h));
}

void vdb_circle(float x, float y, float r)
{
    vdb_push_u08(vdb_mode_circle);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
    vdb_push_r32(r);
}

void vdb_imageRGB8(const void *data, int w, int h)
{
    vdb_push_u08(vdb_mode_image_rgb8);
    vdb_push_style();
    vdb_push_u32(w);
    vdb_push_u32(h);
    vdb_push_bytes(data, w*h*3);
}

void vdb_slider1f(const char *in_label, float *x, float min_value, float max_value)
{
    vdb_label_t label = {0};
    vdb_copy_label(&label, in_label);
    vdb_push_u08(vdb_mode_slider_float);
    vdb_push_style();
    vdb_push_bytes(label.chars, VDB_LABEL_LENGTH);
    vdb_push_r32(*x);
    vdb_push_r32(min_value);
    vdb_push_r32(max_value);

    // Update variable
    // @ todo: robustness: mutex on msg
    {
        vdb_label_t *msg_label = vdb_shared->msg_var_r32_label;
        float       *msg_value = vdb_shared->msg_var_r32_value;
        int          msg_count = vdb_shared->msg_var_r32_count;
        int i = 0;
        for (i = 0; i < msg_count; i++)
        {
            if (vdb_cmp_label(&msg_label[i], &label))
                *x = msg_value[i];
        }
    }
}
